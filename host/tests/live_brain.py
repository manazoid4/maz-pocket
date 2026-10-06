"""Live acceptance: python tests/live_brain.py [port]. Needs Core running; token via MAZ_TOKEN env. Never prints secrets."""
import os, re, sys, time
from datetime import datetime
from zoneinfo import ZoneInfo
import httpx

port = sys.argv[1] if len(sys.argv) > 1 else "8792"
base = f"http://127.0.0.1:{port}"
h = {"Authorization": "Bearer " + os.environ["MAZ_TOKEN"]}
c = httpx.Client(base_url=base, headers=h, timeout=60)
sid = c.post("/session/start").json()["session_id"]
now = datetime.now(ZoneInfo("Europe/London"))
BAD = re.compile(r"i don'?t have access|as an ai|i can'?t|i cannot|i'?m unable|i am unable|not able to", re.I)
checks = {
    "what time is it": lambda r: bool(re.search(rf"\b{now.hour % 12 or 12}[:.]?(\d\d)?\b", r)) and abs(
        int((re.search(r"(\d{1,2})[:.](\d\d)", r) or [0, 0, 99])[2]) - now.minute) <= 2 if re.search(r"\d[:.]\d\d", r) else str(now.hour % 12 or 12) in r,
    "what is today's date": lambda r: str(now.day) in r and now.strftime("%B").lower() in r.lower(),
    "what's the weather today": lambda r: bool(re.search(r"\d", r)),
    "what is 15 percent of 240": lambda r: "36" in r,
    "what should I work on now": lambda r: len(r) > 10,
    "and after that?": lambda r: len(r) > 5,
    "tell me a joke": lambda r: len(r) > 10,
    "what is the capital of Australia": lambda r: "canberra" in r.lower(),
    "remind me what I just asked": lambda r: "capital" in r.lower() or "australia" in r.lower(),
    "how do I boil an egg": lambda r: "egg" in r.lower() or "boil" in r.lower() or "minute" in r.lower(),
}
fails = 0
for q, ok in checks.items():
    t = time.perf_counter()
    r = c.post("/turn/text", json={"text": q, "session_id": sid, "route": "local"}).json()
    ms = round((time.perf_counter() - t) * 1000)
    reply = r.get("reply", str(r))
    bad = bool(BAD.search(reply)); good = ok(reply)
    fails += bad or not good
    print(f"[{'PASS' if good and not bad else 'FAIL'}] {ms}ms {r.get('provider')} | {q} -> {reply}")
print("RESULT:", "PASS" if not fails else f"{fails} FAIL")
sys.exit(1 if fails else 0)
