"""macOS quality of service for the player's threads.

The audio callbacks run Python on CoreAudio's real-time threads, and they need the interpreter lock. When another Python
thread holds it and the system is busy, the OS may run that thread late (or on an efficiency core) and the callback waits
with it: a gap in the sound. Asking for QOS_CLASS_USER_INTERACTIVE keeps the player's threads ahead of ordinary work.
"""
from __future__ import annotations

import ctypes
import sys
import threading
from typing import Callable

QOS_CLASS_USER_INTERACTIVE = 0x21


def interactive() -> bool:
    """Put the calling thread in the user-interactive class; False where that is not possible (not a Mac, or refused)."""
    if sys.platform != "darwin":
        return False
    try:
        libc = ctypes.CDLL(None)
        return libc.pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0) == 0
    except (OSError, AttributeError):
        return False


def thread(target: Callable[[], None], name: str | None = None) -> threading.Thread:
    """A daemon thread that asks for the class first."""
    def run() -> None:
        interactive()
        target()
    return threading.Thread(target=run, name=name, daemon=True)
