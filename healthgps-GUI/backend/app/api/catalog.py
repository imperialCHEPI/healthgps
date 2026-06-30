"""Program catalog endpoints."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from app.services.catalog import CatalogError, list_catalog

router = APIRouter(prefix="/api/catalog", tags=["catalog"])


@router.get("")
def get_catalog() -> dict:
    try:
        return {"programs": list_catalog()}
    except CatalogError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
