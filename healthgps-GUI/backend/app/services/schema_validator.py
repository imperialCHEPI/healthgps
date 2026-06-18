"""Validate config.json against healthgps JSON Schema."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import jsonschema
from jsonschema import Draft202012Validator
from referencing import Registry, Resource

from app.config import get_settings


def _load_schema_file(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def _build_registry(schemas_dir: Path) -> Registry:
    resources: list[tuple[str, Resource]] = []

    def add_schema(file_path: Path, schema_id: str | None = None) -> None:
        schema = _load_schema_file(file_path)
        uri = schema_id or schema.get("$id") or file_path.as_uri()
        resources.append((uri, Resource.from_contents(schema)))

    for path in sorted(schemas_dir.rglob("*.json")):
        try:
            schema = _load_schema_file(path)
            uri = schema.get("$id")
            if uri:
                resources.append((uri, Resource.from_contents(schema)))
        except (json.JSONDecodeError, OSError):
            continue

    # Also register by relative file URI for local $ref resolution
    for path in sorted(schemas_dir.rglob("*.json")):
        rel = path.relative_to(schemas_dir)
        file_uri = (schemas_dir / rel).as_uri()
        schema = _load_schema_file(path)
        resources.append((file_uri, Resource.from_contents(schema)))
        # config/inputs.json style refs
        posix = rel.as_posix()
        resources.append((f"https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/v1/{posix}", Resource.from_contents(schema)))

    return Registry().with_resources(resources)


_validator: Draft202012Validator | None = None


def get_validator() -> Draft202012Validator:
    global _validator
    if _validator is not None:
        return _validator

    settings = get_settings()
    schemas_dir: Path = settings["schemas_dir"]  # type: ignore[assignment]
    config_schema_path = schemas_dir / "config.json"
    if not config_schema_path.is_file():
        raise FileNotFoundError(f"Schema not found: {config_schema_path}")

    registry = _build_registry(schemas_dir)
    schema = _load_schema_file(config_schema_path)
    _validator = Draft202012Validator(schema, registry=registry)
    return _validator


def validate_config(config: dict[str, Any]) -> tuple[bool, list[str]]:
    validator = get_validator()
    errors: list[str] = []
    for error in sorted(validator.iter_errors(config), key=lambda e: e.path):
        path = ".".join(str(p) for p in error.path) if error.path else "(root)"
        errors.append(f"{path}: {error.message}")
    return len(errors) == 0, errors


def validate_config_file(config_path: Path) -> tuple[bool, list[str]]:
    with config_path.open(encoding="utf-8") as f:
        config = json.load(f)
    return validate_config(config)
