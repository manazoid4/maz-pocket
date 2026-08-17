from __future__ import annotations

import hmac
from typing import Annotated

from fastapi import Cookie, FastAPI, Form, HTTPException, Request
from fastapi.responses import HTMLResponse, JSONResponse, RedirectResponse
from pydantic import BaseModel, Field

from .authority import AuthorityBroker, AuthorityError, Scope
from .config import Settings


CONTROL_HEADER = "X-MAZ-Control"
COOKIE = "maz_control_session"


class ManualGrantRequest(BaseModel):
    scope: Scope = "pc_full"
    seconds: int = Field(default=600, ge=30, le=3600)
    task: str = Field(default="Manual phone-authorized MAZ session", min_length=1, max_length=240)


PAGE = r"""<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#080b0e"><title>MAZ Control</title><style>
:root{color-scheme:dark;--b:#080b0e;--p:#11171c;--p2:#0c1115;--l:#27323b;--t:#f2f4f5;--d:#91a0aa;--a:#ff7a18;--g:#42d58a;--r:#ff676d;--w:#ffc04d}*{box-sizing:border-box}body{margin:0;background:var(--b);color:var(--t);font:14px ui-monospace,SFMono-Regular,Consolas,monospace}main{max-width:760px;margin:auto;padding:12px}.top{position:sticky;top:0;z-index:4;background:#080b0ef3;border-bottom:1px solid var(--l);padding:10px 0;display:flex;justify-content:space-between;align-items:center;gap:8px}.brand{font-size:20px;font-weight:900}.brand b{color:var(--a)}.token{font-size:11px;color:var(--d);border:1px solid var(--l);border-radius:999px;padding:5px 8px}.hero{padding:17px 0 7px}.hero h1{font-size:24px;margin:0 0 5px}.hero p{color:var(--d);line-height:1.45;margin:0}.sec{margin-top:15px}.sec h2{font-size:10px;letter-spacing:.14em;color:var(--d)}.card{border:1px solid var(--l);border-radius:10px;background:var(--p);padding:12px;margin:7px 0}.req{border-left:3px solid var(--w)}.grant{border-left:3px solid var(--g)}.kv{display:grid;grid-template-columns:1fr auto;gap:7px 10px}.kv span{color:var(--d)}.row{display:flex;gap:7px;flex-wrap:wrap;margin-top:10px}button,input,select{font:inherit;border:1px solid var(--l);border-radius:7px;background:#080c0f;color:var(--t);padding:9px}button{cursor:pointer;flex:1;min-width:110px}.yes{background:var(--a);border-color:var(--a);color:#08090a;font-weight:900}.danger{border-color:#6a3438;color:var(--r)}.muted{color:var(--d);font-size:12px;line-height:1.4}.bad{color:var(--r)}.good{color:var(--g)}.warn{color:var(--w)}pre{white-space:pre-wrap;word-break:break-word;background:var(--p2);border-radius:7px;padding:9px;font-size:11px;color:var(--d);max-height:220px;overflow:auto}.empty{padding:16px;text-align:center;color:var(--d);border:1px dashed var(--l);border-radius:9px}@media(max-width:520px){main{padding:9px}.hero h1{font-size:20px}.kv{grid-template-columns:1fr}.kv b{text-align:left}.top{align-items:flex-start;flex-direction:column}}
</style></head><body><main>
<div class="top"><div class="brand"><b>MAZ</b> CONTROL</div><div class="token">TOKEN ID <b id="tokenId">-</b></div></div>
<div class="hero"><h1>Phone approval centre</h1><p>Agents can request power here. They cannot approve themselves. Confirm a scoped, expiring grant, watch the action feed, or revoke everything instantly.</p></div>
<div class="sec"><h2>EMERGENCY</h2><div class="card"><div class="row"><button class="danger" onclick="revokeAll()">REVOKE ALL CONTROL</button><button onclick="refreshAll()">REFRESH</button></div><div id="status" class="muted" style="margin-top:8px">Loading...</div></div></div>
<div class="sec"><h2>PENDING APPROVALS</h2><div id="pending"></div></div>
<div class="sec"><h2>ACTIVE GRANTS</h2><div id="grants"></div></div>
<div class="sec"><h2>MANUAL SESSION</h2><div class="card"><p class="muted">Use this when you intentionally want MAZ to have broad control before an agent asks.</p><div class="row"><select id="manualScope"><option value="project_full">PROJECT FULL</option><option value="pc_full" selected>PC FULL</option><option value="admin">PC FULL + ADMIN GRANT</option></select><select id="manualSeconds"><option value="300">5 min</option><option value="600" selected>10 min</option><option value="900">15 min</option><option value="1800">30 min</option></select><button class="yes" onclick="manualGrant()">CONFIRM GRANT</button></div></div></div>
<div class="sec"><h2>ACTION FEED</h2><div class="card"><pre id="audit">No actions yet.</pre></div></div>
<div class="sec"><h2>SESSION</h2><div class="card"><div class="row"><button onclick="logout()">LOG OUT THIS PHONE</button></div><p class="muted">The pairing token is never displayed here. TOKEN ID is a non-secret fingerprint so you can tell which MAZ installation you are authorising.</p></div></div>
<script>
const H={'X-MAZ-Control':'1','Content-Type':'application/json'};const $=x=>document.getElementById(x);function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}async function api(path,opt={}){let r=await fetch(path,{cache:'no-store',...opt,headers:{...H,...(opt.headers||{})}});if(r.status===401){location.href='./';throw new Error('login required')}let d=await r.json();if(!r.ok)throw new Error(d.detail||d.error||'request failed');return d}function secs(t){return Math.max(0,Math.round(t-Date.now()/1000))+'s'}function reqCard(x){let cmds=(x.commands_preview||[]).map(esc).join('\n');let files=(x.files||[]).map(esc).join('\n');return `<div class="card req"><div class="kv"><span>TASK</span><b>${esc(x.task)}</b><span>AGENT</span><b>${esc(x.agent)}</b><span>ACCESS</span><b class="warn">${esc(x.scope).toUpperCase()}</b><span>PROJECT</span><b>${esc(x.project||'whole PC')}</b><span>DURATION</span><b>${x.duration_seconds}s</b><span>ADMIN</span><b>${x.requires_admin?'YES':'NO'}</b></div>${cmds?`<pre>${cmds}</pre>`:''}${files?`<pre>${files}</pre>`:''}<div class="row"><button class="danger" onclick="deny('${x.request_id}')">DENY</button><button class="yes" onclick="approve('${x.request_id}')">CONFIRM</button></div></div>`}function grantCard(x){return `<div class="card grant"><div class="kv"><span>TASK</span><b>${esc(x.task)}</b><span>AGENT</span><b>${esc(x.agent)}</b><span>ACCESS</span><b class="good">${esc(x.scope).toUpperCase()}</b><span>PROJECT</span><b>${esc(x.project||'whole PC')}</b><span>REMAINING</span><b>${secs(x.expires_at)}</b></div><div class="row"><button class="danger" onclick="revoke('${x.grant_id}')">REVOKE</button></div></div>`}async function refreshAll(){try{let d=await api('api/state');$('tokenId').textContent=d.token_id;$('pending').innerHTML=d.pending.length?d.pending.map(reqCard).join(''):'<div class="empty">No approval waiting.</div>';$('grants').innerHTML=d.grants.length?d.grants.map(grantCard).join(''):'<div class="empty">No elevated control active.</div>';$('status').innerHTML=`<span class="good">PHONE AUTHENTICATED</span> · ${d.pending.length} pending · ${d.grants.length} active`;$('audit').textContent=(d.audit||[]).slice().reverse().map(x=>new Date(x.at*1000).toLocaleTimeString()+'  '+x.event+'  '+JSON.stringify(x.details)).join('\n')||'No actions yet.'}catch(e){$('status').innerHTML='<span class="bad">'+esc(e.message)+'</span>'}}async function approve(id){await api('api/approve/'+encodeURIComponent(id),{method:'POST'});refreshAll()}async function deny(id){await api('api/deny/'+encodeURIComponent(id),{method:'POST'});refreshAll()}async function revoke(id){await api('api/revoke/'+encodeURIComponent(id),{method:'POST'});refreshAll()}async function revokeAll(){await api('api/revoke-all',{method:'POST'});refreshAll()}async function manualGrant(){await api('api/manual-grant',{method:'POST',body:JSON.stringify({scope:$('manualScope').value,seconds:Number($('manualSeconds').value),task:'Manual phone-authorized MAZ session'})});refreshAll()}async function logout(){await api('api/logout',{method:'POST'});location.href='./'}refreshAll();setInterval(refreshAll,3000);
</script></main></body></html>"""


LOGIN = r"""<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="theme-color" content="#080b0e"><title>MAZ Control Login</title><style>:root{color-scheme:dark}body{margin:0;background:#080b0e;color:#f2f4f5;font:14px ui-monospace,Consolas,monospace;display:grid;min-height:100vh;place-items:center}.c{width:min(92vw,420px);border:1px solid #27323b;background:#11171c;border-radius:12px;padding:18px}b{color:#ff7a18}p{color:#91a0aa;line-height:1.45}input,button{width:100%;font:inherit;padding:11px;border-radius:8px;border:1px solid #27323b;background:#080c0f;color:#f2f4f5;margin-top:8px}button{background:#ff7a18;color:#08090a;font-weight:900}</style></head><body><form class="c" method="post" action="session/login"><h1><b>MAZ</b> CONTROL</h1><p>Enter the MAZ pairing token once on this phone. A short-lived device-bound session cookie is used afterwards.</p><input name="token" type="password" autocomplete="current-password" placeholder="MAZ pairing token" required><button>UNLOCK PHONE APPROVALS</button></form></body></html>"""


def build_phone_app(settings: Settings, broker: AuthorityBroker) -> FastAPI:
    phone = FastAPI(title="MAZ Phone Control", docs_url=None, redoc_url=None, openapi_url=None)

    def require_session(request: Request, session: str | None) -> None:
        if request.headers.get(CONTROL_HEADER) != "1":
            raise HTTPException(403, "control_header_required")
        if not session:
            raise HTTPException(401, "phone_login_required")
        try:
            broker.verify_phone_session(session, request.headers.get("user-agent", ""))
        except AuthorityError as error:
            raise HTTPException(401, str(error)) from error

    @phone.get("/", response_class=HTMLResponse)
    def home(request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        if maz_control_session:
            try:
                broker.verify_phone_session(
                    maz_control_session, request.headers.get("user-agent", "")
                )
                return HTMLResponse(PAGE)
            except AuthorityError:
                pass
        return HTMLResponse(LOGIN)

    @phone.post("/session/login")
    def login(request: Request, token: Annotated[str, Form()]):
        if not settings.token_configured:
            raise HTTPException(503, "MAZ_TOKEN_not_configured")
        if not hmac.compare_digest(token, settings.token):
            raise HTTPException(401, "invalid_pairing_token")
        signed = broker.issue_phone_session(request.headers.get("user-agent", ""))
        response = RedirectResponse(url="./", status_code=303)
        response.set_cookie(
            COOKIE,
            signed,
            httponly=True,
            samesite="strict",
            secure=request.url.scheme == "https",
            max_age=settings.control_phone_session_seconds,
            path="/control",
        )
        return response

    @phone.get("/api/state")
    def state(request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        return {
            "ok": True,
            "token_id": broker.token_id,
            "pending": broker.pending(),
            "grants": broker.active_grants(),
            "audit": broker.recent_audit(60),
        }

    @phone.post("/api/approve/{request_id}")
    def approve(request_id: str, request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        try:
            result = broker.approve(request_id)
        except AuthorityError as error:
            raise HTTPException(400, str(error)) from error
        result.pop("grant_token", None)
        return {"ok": True, "request": result}

    @phone.post("/api/deny/{request_id}")
    def deny(request_id: str, request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        try:
            return {"ok": True, "request": broker.deny(request_id)}
        except AuthorityError as error:
            raise HTTPException(400, str(error)) from error

    @phone.post("/api/revoke/{grant_id}")
    def revoke(grant_id: str, request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        try:
            return {"ok": True, "grant": broker.revoke(grant_id)}
        except AuthorityError as error:
            raise HTTPException(400, str(error)) from error

    @phone.post("/api/revoke-all")
    def revoke_all(request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        return {"ok": True, "revoked": broker.revoke_all()}

    @phone.post("/api/manual-grant")
    def manual_grant(body: ManualGrantRequest, request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        scope: Scope = body.scope
        requires_admin = scope == "admin"
        item = broker.request(
            task=body.task,
            agent="any",
            scope=scope,
            duration_seconds=body.seconds,
            requires_admin=requires_admin,
        )
        approved = broker.approve(item["request_id"])
        approved.pop("grant_token", None)
        return {"ok": True, "request": approved}

    @phone.post("/api/logout")
    def logout(request: Request, maz_control_session: Annotated[str | None, Cookie()] = None):
        require_session(request, maz_control_session)
        response = JSONResponse({"ok": True})
        response.delete_cookie(COOKIE, path="/control")
        return response

    return phone
