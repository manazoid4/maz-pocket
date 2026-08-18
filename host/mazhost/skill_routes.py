from __future__ import annotations

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from .llm import Route
from .skill_vault import SkillError, SkillVault


class TeachSkillRequest(BaseModel):
    session_id: str = Field(min_length=8, max_length=80)
    route: Route = "auto"


class ApproveSkillRequest(BaseModel):
    replace: bool = False


def install_skill_routes(api: FastAPI, vault: SkillVault) -> None:
    @api.post("/skills/draft/from-teach")
    def skill_from_teach(body: TeachSkillRequest):
        try:
            return {"ok": True, "draft": vault.draft_from_teach(body.session_id, body.route)}
        except SkillError as error:
            raise HTTPException(400, str(error)) from error

    @api.get("/skills/draft/{draft_id}")
    def skill_draft(draft_id: str):
        try:
            return {"ok": True, "draft": vault.draft(draft_id)}
        except SkillError as error:
            raise HTTPException(404, str(error)) from error

    @api.post("/skills/draft/{draft_id}/approve")
    def skill_approve(draft_id: str, body: ApproveSkillRequest):
        # This endpoint never runs automatically. A caller must explicitly
        # promote the draft after reviewing it; Teach STOP only produces source
        # evidence, and `/from-teach` only produces a draft.
        try:
            return {"ok": True, "skill": vault.approve(draft_id, replace=body.replace)}
        except SkillError as error:
            raise HTTPException(400, str(error)) from error

    @api.get("/skills")
    def skills_list():
        return {"ok": True, "skills": vault.list_skills()}
