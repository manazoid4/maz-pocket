from __future__ import annotations

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from .look import LookError, LookService


class LookRequest(BaseModel):
    question: str = Field(default="What am I looking at?", min_length=1, max_length=1200)
    display_id: str = Field(default="primary", max_length=120)


def install_look_routes(api: FastAPI, service: LookService) -> None:
    @api.post("/look")
    def look(body: LookRequest):
        try:
            return service.look(body.question, body.display_id)
        except LookError as error:
            raise HTTPException(400, str(error)) from error
