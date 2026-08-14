"""Transcript cleanup.

Ports OpenFlowKit's `deterministicRefine` contract (src/core/refine.ts): same
pipeline, same {provider, text, actions} result shape, so OpenFlowKit can later
take this job over without anything upstream changing.
"""

from __future__ import annotations

from mazhost.refine import refine


def test_strips_fillers_and_reports_what_it_did():
    result = refine("um so I basically want to, you know, finish the mic test")
    assert "um" not in result.text.lower().split()
    assert "basically" not in result.text.lower()
    assert "removed fillers" in result.actions
    assert result.provider == "deterministic-local"
