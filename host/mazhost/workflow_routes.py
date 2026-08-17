from __future__ import annotations

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from .llm import Route
from .workflows import WorkflowService


class CompilePromptRequest(BaseModel):
    template_id: str = Field(min_length=1, max_length=80)
    task: str = Field(min_length=1, max_length=8000)
    project: str = Field(default="", max_length=160)


class PlanRequest(BaseModel):
    task: str = Field(min_length=1, max_length=8000)
    project: str = Field(default="", max_length=160)
    route: Route = "auto"


class CrewRequest(BaseModel):
    task: str = Field(min_length=1, max_length=8000)
    project: str = Field(default="", max_length=160)
    route: Route = "auto"


class RetroRequest(BaseModel):
    project: str = Field(default="", max_length=160)
    note: str = Field(default="", max_length=6000)
    route: Route = "auto"


def install_workflow_routes(api: FastAPI, service: WorkflowService) -> None:
    @api.get("/work/templates")
    def templates():
        return {"ok": True, "templates": service.templates()}

    @api.post("/work/prompt")
    def compile_prompt(body: CompilePromptRequest):
        try:
            return service.compile_prompt(body.template_id, body.task, body.project)
        except ValueError as error:
            raise HTTPException(404, str(error)) from error

    @api.post("/work/plan")
    def plan(body: PlanRequest):
        try:
            return service.plan(body.task, body.project, body.route)
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error

    @api.post("/work/crew")
    def crew(body: CrewRequest):
        try:
            return service.crew(body.task, body.project, body.route)
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error

    @api.post("/work/retro")
    def retro(body: RetroRequest):
        try:
            return service.retro(body.project, body.route, body.note)
        except RuntimeError as error:
            raise HTTPException(503, str(error)) from error
