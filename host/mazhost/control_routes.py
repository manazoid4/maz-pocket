from __future__ import annotations

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from .authority import AuthorityBroker, AuthorityError, Scope
from .debug_capsule import DebugCapsules
from .executor import ElevatedExecutor, ExecutionError, Shell
from .phone_control import build_phone_app
from .workflow_routes import install_workflow_routes
from .workflows import WorkflowService


class AuthorityRequestBody(BaseModel):
    task: str = Field(min_length=1, max_length=400)
    agent: str = Field(default="unknown", min_length=1, max_length=120)
    scope: Scope
    project: str = Field(default="", max_length=300)
    duration_seconds: int = Field(default=600, ge=30, le=3600)
    commands_preview: list[str] = Field(default_factory=list, max_length=20)
    files: list[str] = Field(default_factory=list, max_length=40)
    network_targets: list[str] = Field(default_factory=list, max_length=20)
    requires_admin: bool = False


class ExecuteBody(BaseModel):
    grant_token: str = Field(min_length=20, max_length=5000)
    command: str = Field(min_length=1, max_length=20_000)
    cwd: str = Field(default="", max_length=1000)
    shell: Shell = "powershell"
    timeout_seconds: int = Field(default=120, ge=1, le=3600)
    agent: str = Field(default="", max_length=120)
    requires_admin: bool = False


class DebugRequest(BaseModel):
    project: str = Field(default="", max_length=160)


def install_control_routes(
    api: FastAPI,
    *,
    settings,
    broker: AuthorityBroker,
    executor: ElevatedExecutor,
    capsules: DebugCapsules,
    core_service,
    core_jobs,
    system_telemetry,
    model_router,
    nudge_client,
    device_monitor,
) -> None:
    # Mounted app has its own phone-session authentication and intentionally
    # does not inherit the model-facing bearer-token dependency.
    api.mount("/control", build_phone_app(settings, broker))

    # Prompt Deck / PLAN / CREW / RETRO share current project + Nudge evidence
    # and the already-configured model router. They do not get a second agent
    # framework or scheduler.
    install_workflow_routes(
        api, WorkflowService(settings, model_router, core_service, nudge_client)
    )

    @api.get("/authority/token-id")
    def authority_token_id():
        return {"token_id": broker.token_id, "phone_url": "/control/"}

    @api.post("/authority/request")
    def authority_request(body: AuthorityRequestBody):
        project = body.project
        if body.scope == "project_full":
            try:
                project = str(core_service.project(body.project)["path"])
            except Exception as error:
                raise HTTPException(400, f"project_not_found:{body.project}") from error
        try:
            return broker.request(
                task=body.task,
                agent=body.agent,
                scope=body.scope,
                project=project,
                duration_seconds=body.duration_seconds,
                commands_preview=body.commands_preview,
                files=body.files,
                network_targets=body.network_targets,
                requires_admin=body.requires_admin,
            )
        except AuthorityError as error:
            raise HTTPException(400, str(error)) from error

    @api.get("/authority/request/{request_id}")
    def authority_request_status(request_id: str):
        try:
            return broker.request_status(request_id, reveal_grant=True)
        except AuthorityError as error:
            raise HTTPException(404, str(error)) from error

    @api.get("/authority/grants")
    def authority_grants():
        return {"ok": True, "grants": broker.active_grants()}

    @api.post("/authority/revoke/{grant_id}")
    def authority_revoke(grant_id: str):
        # Revocation is always safe to expose to the normal authenticated MAZ
        # control plane; it removes power rather than granting it.
        try:
            return {"ok": True, "grant": broker.revoke(grant_id)}
        except AuthorityError as error:
            raise HTTPException(404, str(error)) from error

    @api.post("/authority/revoke-all")
    def authority_revoke_all():
        return {"ok": True, "revoked": broker.revoke_all()}

    @api.post("/authority/execute")
    def authority_execute(body: ExecuteBody):
        try:
            return executor.run(
                grant_token=body.grant_token,
                command=body.command,
                cwd=body.cwd,
                shell=body.shell,
                timeout_seconds=body.timeout_seconds,
                agent=body.agent,
                requires_admin=body.requires_admin,
            )
        except ExecutionError as error:
            raise HTTPException(403, str(error)) from error

    @api.post("/debug/capsule")
    def debug_capsule(body: DebugRequest):
        project = body.project
        return capsules.collect(
            project=project,
            core_status=core_service.status,
            project_status=core_service.project,
            jobs_status=lambda: {"jobs": core_jobs.recent(20)},
            system_status=system_telemetry.snapshot,
            model_status=model_router.status,
            nudge_status=nudge_client.status,
            device_status=device_monitor.status,
            authority_status=lambda: {
                "token_id": broker.token_id,
                "pending": len(broker.pending()),
                "active_grants": broker.active_grants(),
            },
        )

    @api.get("/debug/capsules")
    def debug_capsules():
        return {"ok": True, "capsules": capsules.recent()}

    @api.get("/debug/capsule/{capsule_id}")
    def debug_capsule_get(capsule_id: str):
        try:
            return capsules.get(capsule_id)
        except (ValueError, FileNotFoundError) as error:
            raise HTTPException(404, str(error)) from error
