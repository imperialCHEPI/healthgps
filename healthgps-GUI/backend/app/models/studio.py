"""Pydantic models for HealthGPS Studio API."""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field


class DemographicsRequirements(BaseModel):
    age: bool = True
    gender: bool = True
    region: bool = False
    ethnicity: bool = False
    max_age_for_linear_models: int | None = None


class IncomeRequirements(BaseModel):
    enabled: bool = True
    type: str = "categorical"
    categories: str = "3"
    adjust_to_factors_mean: bool = False
    trended: bool = False
    income_based_csv_output: bool = True


class PhysicalActivityRequirements(BaseModel):
    enabled: bool = True
    type: str = "simple"
    adjust_to_factors_mean: bool = False
    trended: bool = False


class RiskFactorsRequirements(BaseModel):
    adjust_to_factors_mean: bool = True
    trended: bool = True


class TrendRequirements(BaseModel):
    enabled: bool = False
    type: str = "null"


class TwoStageRequirements(BaseModel):
    use_logistic: bool = False


class ProjectRequirementsState(BaseModel):
    demographics: DemographicsRequirements
    income: IncomeRequirements
    physical_activity: PhysicalActivityRequirements
    risk_factors: RiskFactorsRequirements
    trend: TrendRequirements
    two_stage: TwoStageRequirements


class RunSettings(BaseModel):
    size_fraction: float = Field(default=0.0001, gt=0, le=1.0)
    age_range_min: int = 0
    age_range_max: int = 110
    start_time: int = 2022
    stop_time: int = 2025
    trial_runs: int = Field(default=1, ge=1)
    active_intervention: str = ""
    thread_count: int = Field(default=4, ge=1)
    enabled_risk_factors: list[str] = Field(default_factory=list)


class ConfigOption(BaseModel):
    id: str
    label: str
    file: str
    path: str | None = None
    exists: bool = True


class WorkspaceCreateRequest(BaseModel):
    project_id: str
    config_variant: str = "config"
    project_requirements: ProjectRequirementsState
    run_settings: RunSettings
    pif_enabled: bool | None = None


class CountryOption(BaseModel):
    id: str
    name: str
    has_example_data: bool = False
    project_id: str | None = None


class NewUserSessionRequest(BaseModel):
    country_id: str
    country_name: str
    population_label: str = ""
    project_requirements: ProjectRequirementsState
    run_settings: RunSettings


class ConsentRequest(BaseModel):
    consent_acknowledged: bool = False


class ProjectSummary(BaseModel):
    id: str
    name: str
    description: str
    has_pif: bool
    locked_fields: list[str]


class ProjectDetail(ProjectSummary):
    examples_root: str
    example_dir_path: str
    default_config_variant: str
    default_config_path: str
    config_options: list[ConfigOption]
    default_project_requirements: dict[str, Any]
    model_risk_factors: list[str]
    local_defaults: dict[str, Any]
    intervention_ids: list[str]


class WorkspaceMeta(BaseModel):
    id: str
    project_id: str
    config_variant: str = "config"
    source_config_path: str
    active_config_path: str
    path: str
    created_at: str
    project_requirements: dict[str, Any]
    run_settings: dict[str, Any]
    last_run_status: str | None = None
    session_type: str | None = None
    country_id: str | None = None
    country_name: str | None = None
    population_label: str | None = None
    session_label: str | None = None


class SchemaValidationError(BaseModel):
    field: str
    message: str
    validator: str
    expected: str | None = None
    supplied: str | None = None
    summary: str


class SchemaValidationResult(BaseModel):
    valid: bool
    errors: list[str] = Field(default_factory=list)
    error_details: list[SchemaValidationError] = Field(default_factory=list)


class RunStatus(BaseModel):
    state: str
    exit_code: int | None = None
    log_tail: str = ""
    terminal_launched: bool = False
    command: str | None = None


class AgeBin(BaseModel):
    label: str
    count: int


class PhaseStep(BaseModel):
    id: str
    label: str
    progress_pct: float
    status: str


class RunTelemetry(BaseModel):
    state: str
    phase: str
    phase_message: str
    current_year: int | None = None
    start_year: int
    stop_year: int
    year_progress_pct: float
    population_initialized: int
    target_population: int
    population_source: str = "pending"
    size_fraction_pct: float = 0.0
    policy_label: str
    cpu_percent: float
    memory_mb: float
    cpu_history: list[float]
    memory_history: list[float]
    gender_male_pct: float
    gender_female_pct: float
    age_bins: list[AgeBin]
    enabled_attributes: list[str]
    events: list[str]
    phase_steps: list[PhaseStep] = Field(default_factory=list)
    dry_run: bool = False


class SettingsResponse(BaseModel):
    healthgps_console: str | None
    healthgps_root: str
    examples_root: str
    workspaces_root: str
