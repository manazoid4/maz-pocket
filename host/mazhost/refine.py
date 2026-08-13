"""Transcript cleanup, ported from OpenFlowKit's `deterministicRefine`.

Whisper output is accurate but spoken: fillers, restarts, doubled words. Cleaning
it deterministically before the model sees it costs nothing, is reproducible, and
means a local 3B model is not spending its attention on "um".

The {provider, text, actions} shape matches OpenFlowKit's `RefinementResult`
exactly, so swapping this for a call into OpenFlowKit later changes one function
body and nothing else.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

FILLER = re.compile(
    r"\b(um+|uh+|erm+|ah+|like|you know|sort of|kind of|i mean|basically"
    r"|literally|actually)\b[,\s]*",
    re.IGNORECASE,
)


@dataclass
class Refinement:
    provider: str
    text: str
    actions: list[str] = field(default_factory=list)


def refine(text: str) -> Refinement:
    cleaned = " ".join(text.strip().split())
    actions: list[str] = []

    without_fillers = " ".join(FILLER.sub(" ", cleaned).split())
    if without_fillers != cleaned:
        cleaned = without_fillers
        actions.append("removed fillers")

    return Refinement(provider="deterministic-local", text=cleaned, actions=actions)
