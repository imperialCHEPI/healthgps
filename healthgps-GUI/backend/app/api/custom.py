"""Custom user flows: new user wizard and expert uploads."""

from __future__ import annotations

from fastapi import APIRouter, File, Form, HTTPException, UploadFile

from app.models.studio import CountryOption, NewUserSessionRequest, WorkspaceMeta
from app.services.custom import (
    CustomSessionError,
    create_expert_session,
    create_new_user_session,
    list_countries,
    new_user_defaults,
)

router = APIRouter(prefix="/api/custom", tags=["custom"])


@router.get("/countries", response_model=list[CountryOption])
def get_countries() -> list[CountryOption]:
    return [CountryOption(**c) for c in list_countries()]


@router.get("/new-user/defaults")
def get_new_user_defaults(country_id: str) -> dict:
    return new_user_defaults(country_id)


@router.post("/new-user", response_model=WorkspaceMeta)
def post_new_user_session(request: NewUserSessionRequest) -> WorkspaceMeta:
    try:
        meta = create_new_user_session(request)
    except Exception as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return WorkspaceMeta(
        **{k: v for k, v in meta.items() if k in WorkspaceMeta.model_fields}
    )


@router.post("/expert", response_model=WorkspaceMeta)
async def post_expert_session(
    country_id: str = Form(...),
    country_name: str = Form(...),
    session_label: str = Form(""),
    config_file: UploadFile = File(...),
    data_files: list[UploadFile] = File(default=[]),
) -> WorkspaceMeta:
    config_bytes = await config_file.read()
    extra: list[tuple[str, bytes]] = []
    for upload in data_files:
        if upload.filename:
            extra.append((upload.filename, await upload.read()))
    try:
        meta = create_expert_session(
            country_id=country_id,
            country_name=country_name,
            session_label=session_label,
            config_bytes=config_bytes,
            config_filename=config_file.filename or "config.json",
            extra_files=extra,
        )
    except CustomSessionError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return WorkspaceMeta(
        **{k: v for k, v in meta.items() if k in WorkspaceMeta.model_fields}
    )
