#!/usr/bin/env python3
"""Drive the real Cardputer through its local USB acceptance surface."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time

import serial
from serial.tools import list_ports


SCREENS = ("talk", "braindump", "inbox", "decision", "focus", "sprint", "nudge", "reminders")


def find_port() -> str:
    for port in list_ports.comports():
        if port.vid == 0x303A and port.pid == 0x1001:
            return port.device
    raise RuntimeError("Cardputer ADV not found over USB")


class Device:
    def __init__(self, port: str):
        self.serial = serial.Serial(port, 115200, timeout=0.25)

    def close(self) -> None:
        self.serial.close()

    def command(self, value: str, prefix: str, timeout: float = 20) -> str:
        self.serial.write((value + "\n").encode())
        self.serial.flush()
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self.serial.readline().decode(errors="replace").strip()
            if line:
                print(f"    < {line}")
            if line.startswith(prefix):
                return line
        raise RuntimeError(f"No {prefix} response to {value!r}")

    def open(self, screen: str) -> None:
        line = self.command(f"MAZOPEN\t{screen}", "MAZOPEN", 45)
        if line != f"MAZOPEN OK screen={screen}":
            raise RuntimeError(line)
        time.sleep(0.35)  # allow the screen's first physical render

    def key(self, name: str, down: bool) -> None:
        self.command(
            f"MAZKEY\t{name}\t{'DOWN' if down else 'UP'}", "MAZKEY", 45
        )

    def press(self, name: str) -> None:
        self.key(name, True)
        time.sleep(0.08)  # match a quick physical key press
        self.key(name, False)

    def type(self, value: str) -> None:
        self.command(f"MAZTYPE\t{value}", "MAZTYPE", 20)

    def state(self, timeout: float = 45) -> dict[str, str]:
        line = self.command("MAZSCREEN", "MAZSCREEN", timeout)
        return dict(item.split("=", 1) for item in line.split()[1:])

    def wait_state(self, predicate, timeout: float, message: str) -> dict[str, str]:
        deadline = time.time() + timeout
        last: dict[str, str] = {}
        while time.time() < deadline:
            last = self.state(min(10, timeout))
            if predicate(last):
                return last
            time.sleep(0.5)
        raise RuntimeError(f"{message}; last state: {last}")


def speak(text: str) -> subprocess.Popen:
    escaped = text.replace("'", "''")
    command = (
        "Add-Type -AssemblyName System.Speech; "
        "$voice = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
        "$voice.Volume = 100; $voice.Rate = 0; "
        f"$voice.Speak('{escaped}')"
    )
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    return subprocess.Popen(
        ["powershell", "-NoProfile", "-Command", command],
        creationflags=flags,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="")
    parser.add_argument("--skip-audio", action="store_true")
    args = parser.parse_args()

    device = Device(args.port or find_port())
    try:
        baseline = device.state()
        print(f"[+] Baseline: {baseline}")

        for screen in SCREENS:
            device.open(screen)
            require(device.state()["screen"] == screen, f"{screen} did not stay open")
        print("[+] All eight first-class screens opened and rendered")

        if not args.skip_audio:
            before = device.state()
            device.open("talk")
            device.key("SPACE", True)
            narrator = speak("What is two plus two? Answer briefly.")
            narrator.wait(timeout=20)
            time.sleep(0.5)
            device.key("SPACE", False)
            after = device.wait_state(
                lambda state: state["recording"] == "0"
                and int(state["inbox"]) > int(before["inbox"]),
                75,
                "Talk did not return an answer to Inbox",
            )
            print("[+] Talk: device mic -> STT -> local model -> Inbox")

            before = device.state()
            device.open("braindump")
            require(device.state()["recording"] == "1", "BrainDump did not record")
            narrator = speak(
                "I decided to use MAZ Pocket today because it keeps capture fast. "
                "The action is to test every shortcut."
            )
            time.sleep(0.8)
            device.press("H")
            narrator.wait(timeout=20)
            device.press("ENTER")
            saved = device.state()
            require(
                int(saved["braindumps"]) > int(before["braindumps"]),
                "BrainDump did not preserve its raw WAV",
            )
            device.press("O")
            processed = device.wait_state(
                lambda state: int(state["inbox"]) > int(before["inbox"]),
                90,
                "BrainDump processing did not produce an Inbox result",
            )
            print("[+] BrainDump: immediate capture, highlight, raw preservation, processing")

        before = device.state()
        device.open("decision")
        device.type("Use MAZ Pocket today")
        device.press("ENTER")
        device.type("It keeps capture and agent control close")
        device.press("ENTER")
        require(
            int(device.state()["decisions"]) > int(before["decisions"]),
            "Decision what + why was not saved",
        )
        print("[+] Decision: what + why saved")

        device.open("focus")
        device.press("ENTER")
        device.type("Try MAZ")
        device.press("ENTER")
        require(device.state()["focus"] == "1", "Focus timer did not start")
        device.press("C")
        require(device.state()["focus"] == "0", "Focus timer did not cancel")
        print("[+] Focus: labelled timer start and cancel")

        before = device.state()
        device.open("sprint")
        device.type("Try every feature")
        device.press("ENTER")
        require(device.state()["focus"] == "1", "Sprint timer did not start")
        require(
            int(device.state()["sprints"]) > int(before["sprints"]),
            "Sprint outcome was not saved",
        )
        device.press("X")
        require(device.state()["focus"] == "0", "Sprint did not stop")
        device.press("B")
        require(device.state()["screen"] == "braindump", "Sprint debrief did not open")
        device.open("home")
        print("[+] Sprint: outcome, timer, stop and voice debrief handoff")

        before = device.state()
        device.open("reminders")
        device.press("A")
        device.type("1 Acceptance check")
        device.press("ENTER")
        require(
            int(device.state()["reminders"]) > int(before["reminders"]),
            "Reminder was not saved",
        )
        print("[+] Reminders: local scheduled reminder saved")

        device.open("nudge")
        require(device.state()["screen"] == "nudge", "Nudge did not render")
        status = device.command("MAZSTATUS", "MAZSTATUS", 45)
        require("host=online" in status and "agents=" in status, "Nudge is not live")
        print("[+] Nudge: authenticated live fleet evidence")

        device.open("inbox")
        device.open("home")
        print("[+] Device left on Home and ready to use")
    finally:
        device.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"[!] Acceptance failed: {error}", file=sys.stderr)
        raise
