"""Curated Windows controls for Maz Pocket.

No arbitrary shell execution lives here. The handheld can only request a small
allow-list of everyday controls that map to Windows virtual keys or the native
lock call. That makes remote use useful without turning MAZ Host into a remote
command prompt.
"""

from __future__ import annotations

import ctypes
import sys
from dataclasses import dataclass


KEYEVENTF_KEYUP = 0x0002
VK_LWIN = 0x5B
VK_D = 0x44
VK_VOLUME_MUTE = 0xAD
VK_VOLUME_DOWN = 0xAE
VK_VOLUME_UP = 0xAF
VK_MEDIA_NEXT_TRACK = 0xB0
VK_MEDIA_PREV_TRACK = 0xB1
VK_MEDIA_PLAY_PAUSE = 0xB3


@dataclass(frozen=True)
class ActionResult:
    action: str
    label: str


class PCController:
    LABELS = {
        "desktop": "Desktop shown",
        "play_pause": "Media toggled",
        "mute": "Mute toggled",
        "volume_down": "Volume down",
        "volume_up": "Volume up",
        "previous_track": "Previous track",
        "next_track": "Next track",
        "lock": "PC locked",
    }

    @property
    def available(self) -> bool:
        return sys.platform == "win32"

    def _tap(self, vk: int) -> None:
        user32 = ctypes.windll.user32
        user32.keybd_event(vk, 0, 0, 0)
        user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)

    def _chord(self, *keys: int) -> None:
        user32 = ctypes.windll.user32
        for key in keys:
            user32.keybd_event(key, 0, 0, 0)
        for key in reversed(keys):
            user32.keybd_event(key, 0, KEYEVENTF_KEYUP, 0)

    def perform(self, action: str) -> ActionResult:
        if action not in self.LABELS:
            raise RuntimeError("pc_action_not_allowed")
        if not self.available:
            raise RuntimeError("pc_control_windows_only")

        if action == "desktop":
            self._chord(VK_LWIN, VK_D)
        elif action == "play_pause":
            self._tap(VK_MEDIA_PLAY_PAUSE)
        elif action == "mute":
            self._tap(VK_VOLUME_MUTE)
        elif action == "volume_down":
            self._tap(VK_VOLUME_DOWN)
        elif action == "volume_up":
            self._tap(VK_VOLUME_UP)
        elif action == "previous_track":
            self._tap(VK_MEDIA_PREV_TRACK)
        elif action == "next_track":
            self._tap(VK_MEDIA_NEXT_TRACK)
        elif action == "lock":
            if not ctypes.windll.user32.LockWorkStation():
                raise RuntimeError("pc_lock_failed")

        return ActionResult(action=action, label=self.LABELS[action])
