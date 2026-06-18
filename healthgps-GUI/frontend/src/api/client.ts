export interface CatalogCountry {
  id: string;
  name: string;
  example_dir?: string;
  project_id?: string;
  example_path?: string;
  available?: boolean;
}

export interface CatalogProgram {
  id: string;
  name: string;
  subtitle?: string;
  status: "active" | "upcoming";
  countries: CatalogCountry[];
}

export interface ProjectSummary {
  id: string;
  name: string;
  description: string;
  has_pif: boolean;
  locked_fields: string[];
}

export interface ConfigOption {
  id: string;
  label: string;
  file: string;
  path: string;
  exists: boolean;
}

export interface ProjectDetail extends ProjectSummary {
  examples_root: string;
  example_dir_path: string;
  default_config_variant: string;
  default_config_path: string;
  config_options: ConfigOption[];
  default_project_requirements: Record<string, unknown>;
  model_risk_factors: string[];
  local_defaults: Record<string, unknown>;
  intervention_ids: string[];
}

export interface DemographicsRequirements {
  age: boolean;
  gender: boolean;
  region: boolean;
  ethnicity: boolean;
  max_age_for_linear_models?: number | null;
}

export interface IncomeRequirements {
  enabled: boolean;
  type: string;
  categories: string;
  adjust_to_factors_mean: boolean;
  trended: boolean;
  income_based_csv_output: boolean;
}

export interface PhysicalActivityRequirements {
  enabled: boolean;
  type: string;
  adjust_to_factors_mean: boolean;
  trended: boolean;
}

export interface RiskFactorsRequirements {
  adjust_to_factors_mean: boolean;
  trended: boolean;
}

export interface TrendRequirements {
  enabled: boolean;
  type: string;
}

export interface TwoStageRequirements {
  use_logistic: boolean;
}

export interface ProjectRequirementsState {
  demographics: DemographicsRequirements;
  income: IncomeRequirements;
  physical_activity: PhysicalActivityRequirements;
  risk_factors: RiskFactorsRequirements;
  trend: TrendRequirements;
  two_stage: TwoStageRequirements;
}

export interface RunSettings {
  size_fraction: number;
  age_range_min: number;
  age_range_max: number;
  start_time: number;
  stop_time: number;
  trial_runs: number;
  active_intervention: string;
  thread_count: number;
  enabled_risk_factors: string[];
}

export interface CountryOption {
  id: string;
  name: string;
  has_example_data: boolean;
  project_id?: string;
}

export interface WorkspaceMeta {
  id: string;
  project_id: string;
  config_variant: string;
  source_config_path: string;
  active_config_path: string;
  path: string;
  created_at: string;
  project_requirements: Record<string, unknown>;
  run_settings: RunSettings;
  last_run_status: string | null;
  session_type?: string;
  country_id?: string;
  country_name?: string;
  population_label?: string;
  session_label?: string;
}

export interface RunStatus {
  state: string;
  exit_code: number | null;
  log_tail: string;
  terminal_launched: boolean;
  command: string | null;
}

export interface AgeBin {
  label: string;
  count: number;
}

export interface PhaseStep {
  id: string;
  label: string;
  progress_pct: number;
  status: string;
}

export interface SchemaValidationError {
  field: string;
  message: string;
  validator: string;
  expected: string | null;
  supplied: string | null;
  summary: string;
}

export interface ScenarioTimeline {
  current_year: number;
  progress_pct: number;
  active: boolean;
  status?: "waiting" | "running" | "complete" | "skipped";
}

export interface PipelineModule {
  id: string;
  label: string;
  description: string;
  status: "pending" | "active" | "done" | "disabled";
  enabled: boolean;
}

export interface HeadlineMetric {
  id: string;
  label: string;
  baseline: number;
  intervention: number;
  delta: number;
  delta_pct: number | null;
  unit: string;
  year: number;
  headline: string;
}

export interface BurdenBar {
  id: string;
  label: string;
  baseline: number;
  intervention: number;
  delta: number;
}

export interface StratumDumbbell {
  stratum: string;
  baseline: number;
  intervention: number;
  delta: number;
}

export interface ComorbidityCell {
  level: string;
  label: string;
  male: number;
  female: number;
  average: number;
}

export interface VisualizationBundle {
  pipeline: { modules: PipelineModule[]; active_module_id: string | null };
  chart_builder: {
    variables: { id: string; label: string; category: string; unit: string }[];
    time_axis: { id: string; label: string; category: string };
    chart_types: { id: string; label: string }[];
    result_file: string | null;
  };
  scenario1: { pipeline: { modules: PipelineModule[] }; validation_hint: string };
  scenario2: {
    headlines: HeadlineMetric[];
    burden_bars: BurdenBar[];
    trajectories: ResultChart[];
    charts: ResultChart[];
    uncertainty_note: string;
  };
  scenario3: {
    dumbbells: StratumDumbbell[];
    outcome: string;
    strata_type: string;
    note: string;
  };
  scenario4: {
    reproducibility: {
      model?: string;
      version?: string;
      seed?: number | string;
      intervention?: string;
      message?: string;
    };
    individual_tracking: { id: string; title: string; status: string; message: string };
  };
  modelling: {
    population_pyramid: {
      year: number;
      male: number;
      female: number;
      male_pct: number;
      female_pct: number;
    } | null;
    comorbidity_matrix: { title: string; cells: ComorbidityCell[] } | null;
    risk_factor_trends: ResultChart[];
    calibration: { id: string; title: string; status: string; message: string };
    convergence: { id: string; title: string; status: string; message: string };
    tornado: { id: string; title: string; status: string; message: string };
    sankey: { id: string; title: string; status: string; message: string };
    live_progress: Record<string, unknown>;
  };
  meta: {
    results_dir: string;
    result_file: string | null;
    years: number[];
    target_year: number;
    intervention: string;
    trial_runs: number;
  };
}

export interface ResultChartPoint {
  x: number;
  y: number;
}

export interface ResultChartSeries {
  name: string;
  color: string;
  points: ResultChartPoint[];
}

export interface ResultChart {
  id: string;
  title: string;
  x_label: string;
  y_label: string;
  series: ResultChartSeries[];
}

export interface ResultChartsResponse {
  charts: ResultChart[];
  experiment: { intervention?: string; model?: string; version?: string };
  years: number[];
  results_dir: string;
  result_file?: string;
  message: string | null;
}

export interface RunTelemetry {
  state: string;
  phase: string;
  phase_message: string;
  current_year: number | null;
  start_year: number;
  stop_year: number;
  year_progress_pct: number;
  population_initialized: number;
  target_population: number;
  population_source: string;
  size_fraction_pct: number;
  policy_label: string;
  cpu_percent: number;
  memory_mb: number;
  cpu_history: number[];
  memory_history: number[];
  gender_male_pct: number;
  gender_female_pct: number;
  age_bins: AgeBin[];
  enabled_attributes: string[];
  events: string[];
  phase_steps: PhaseStep[];
  dry_run: boolean;
  baseline_timeline?: ScenarioTimeline | null;
  intervention_timeline?: ScenarioTimeline | null;
  pipeline?: { modules: PipelineModule[]; active_module_id: string | null } | null;
}

export interface SettingsResponse {
  healthgps_console: string | null;
  healthgps_root: string;
  workspaces_root: string;
}

const REQUEST_TIMEOUT_MS = 8000;

export const FALLBACK_COUNTRIES: CountryOption[] = [
  { id: "fra", name: "France", has_example_data: true, project_id: "hlm_france" },
  { id: "ind", name: "India", has_example_data: true, project_id: "india" },
  { id: "gbr", name: "United Kingdom", has_example_data: true, project_id: "finch" },
  { id: "est", name: "Estonia", has_example_data: false },
  { id: "bel", name: "Belgium", has_example_data: false },
  { id: "ita", name: "Italy", has_example_data: false },
  { id: "esp", name: "Spain", has_example_data: false },
];

export const FALLBACK_CATALOG: CatalogProgram[] = [
  {
    id: "stop",
    name: "STOP",
    subtitle: "Childhood obesity policy simulation",
    status: "active",
    countries: [
      { id: "fra", name: "France", project_id: "hlm_france", available: true },
    ],
  },
  {
    id: "resolve",
    name: "Resolve to Save Lives",
    subtitle: "India NCD policy modelling",
    status: "active",
    countries: [{ id: "ind", name: "India", project_id: "india", available: true }],
  },
  {
    id: "finch",
    name: "FINCH",
    subtitle: "UK food and health policy",
    status: "active",
    countries: [
      { id: "gbr", name: "United Kingdom", project_id: "finch", available: true },
    ],
  },
];

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  try {
    const res = await fetch(path, {
      headers: { "Content-Type": "application/json", ...init?.headers },
      signal: controller.signal,
      ...init,
    });
    if (!res.ok) {
      const body = await res.json().catch(() => ({}));
      throw new Error(
        typeof body.detail === "string"
          ? body.detail
          : res.statusText || `Request failed (${res.status})`
      );
    }
    return res.json() as Promise<T>;
  } catch (e) {
    if (e instanceof DOMException && e.name === "AbortError") {
      throw new Error(
        "Backend did not respond in time. Start the API: uvicorn app.main:app --port 8000"
      );
    }
    throw e;
  } finally {
    window.clearTimeout(timer);
  }
}

export const api = {
  getSettings: () => request<SettingsResponse>("/api/settings"),
  catalog: async (): Promise<{ programs: CatalogProgram[]; offline: boolean }> => {
    try {
      const data = await request<{ programs: CatalogProgram[] }>("/api/catalog");
      return { programs: data.programs, offline: false };
    } catch {
      return { programs: FALLBACK_CATALOG, offline: true };
    }
  },
  listProjects: () => request<ProjectSummary[]>("/api/projects"),
  getProject: (id: string, configVariant = "config") =>
    request<ProjectDetail>(
      `/api/projects/${id}?config_variant=${encodeURIComponent(configVariant)}`
    ),
  getWorkspace: (id: string) => request<WorkspaceMeta>(`/api/workspaces/${id}`),
  createWorkspace: (body: {
    project_id: string;
    config_variant: string;
    project_requirements: ProjectRequirementsState;
    run_settings: RunSettings;
    pif_enabled?: boolean | null;
  }) =>
    request<WorkspaceMeta>("/api/workspaces", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  updateWorkspace: (
    id: string,
    body: {
      project_id: string;
      config_variant: string;
      project_requirements: ProjectRequirementsState;
      run_settings: RunSettings;
      pif_enabled?: boolean | null;
    }
  ) =>
    request<WorkspaceMeta>(`/api/workspaces/${id}`, {
      method: "PUT",
      body: JSON.stringify(body),
    }),
  validateSchema: (id: string) =>
    request<{
      valid: boolean;
      errors: string[];
      error_details?: SchemaValidationError[];
    }>(`/api/workspaces/${id}/validate-schema`, { method: "POST" }),
  validate: (id: string, consent_acknowledged: boolean) =>
    request<Record<string, unknown>>(`/api/workspaces/${id}/validate`, {
      method: "POST",
      body: JSON.stringify({ consent_acknowledged }),
    }),
  run: (id: string, consent_acknowledged: boolean) =>
    request<RunStatus>(`/api/workspaces/${id}/run`, {
      method: "POST",
      body: JSON.stringify({ consent_acknowledged }),
    }),
  runStatus: (id: string) =>
    request<RunStatus>(`/api/workspaces/${id}/run/status`),
  runTelemetry: (id: string) =>
    request<RunTelemetry>(`/api/workspaces/${id}/run/telemetry`),
  previewCommand: (id: string, dryRun = false) =>
    request<{ command: string }>(
      `/api/workspaces/${id}/preview-command?dry_run=${dryRun}`
    ),
  results: (id: string) =>
    request<{
      results_dir: string;
      configured_folder?: string;
      searched_dirs?: string[];
      run_timestamp: string | null;
      files: { name: string; path: string; size_bytes?: number; exists?: boolean }[];
    }>(`/api/workspaces/${id}/results`),
  resultCharts: (id: string) =>
    request<ResultChartsResponse>(`/api/workspaces/${id}/results/charts`),
  resultSeries: (id: string, variable: string, sources = "Baseline,Intervention") =>
    request<{ variable: string; sources: string[]; series: ResultChartSeries[] }>(
      `/api/workspaces/${id}/results/series?variable=${encodeURIComponent(variable)}&sources=${encodeURIComponent(sources)}`
    ),
  resultChart: (
    id: string,
    opts: { x?: string; y: string; chartType?: string; sources?: string }
  ) => {
    const params = new URLSearchParams();
    params.set("y", opts.y);
    if (opts.x) params.set("x", opts.x);
    if (opts.chartType) params.set("chart_type", opts.chartType);
    if (opts.sources) params.set("sources", opts.sources);
    return request<{
      title: string;
      x_label: string;
      y_label: string;
      chart_type: string;
      x_var: string;
      y_var: string;
      series: ResultChartSeries[];
    }>(`/api/workspaces/${id}/results/chart?${params.toString()}`);
  },
  visualizations: (id: string) =>
    request<VisualizationBundle>(`/api/workspaces/${id}/visualizations`),
  countries: async () => {
    try {
      return await request<CountryOption[]>("/api/custom/countries");
    } catch {
      return FALLBACK_COUNTRIES;
    }
  },
  newUserDefaults: (countryId: string) =>
    request<{
      country_id: string;
      project_id: string;
      project_name: string;
      default_project_requirements: Record<string, unknown>;
      model_risk_factors: string[];
      local_defaults: Record<string, unknown>;
    }>(`/api/custom/new-user/defaults?country_id=${encodeURIComponent(countryId)}`),
  createNewUserSession: (body: {
    country_id: string;
    country_name: string;
    population_label: string;
    project_requirements: ProjectRequirementsState;
    run_settings: RunSettings;
  }) =>
    request<WorkspaceMeta>("/api/custom/new-user", {
      method: "POST",
      body: JSON.stringify(body),
    }),
  createExpertSession: (body: {
    country_id: string;
    country_name: string;
    session_label: string;
    config_file: File;
    data_files: File[];
  }) => {
    const form = new FormData();
    form.append("country_id", body.country_id);
    form.append("country_name", body.country_name);
    form.append("session_label", body.session_label);
    form.append("config_file", body.config_file);
    for (const f of body.data_files) {
      form.append("data_files", f);
    }
    return fetch("/api/custom/expert", { method: "POST", body: form }).then(
      async (res) => {
        if (!res.ok) {
          const err = await res.json().catch(() => ({}));
          throw new Error(err.detail || res.statusText);
        }
        return res.json() as Promise<WorkspaceMeta>;
      }
    );
  },
};

export const LEGACY_DEFAULT_REQUIREMENTS: ProjectRequirementsState = {
  demographics: {
    age: true,
    gender: true,
    region: false,
    ethnicity: false,
  },
  income: {
    enabled: true,
    type: "categorical",
    categories: "3",
    adjust_to_factors_mean: false,
    trended: false,
    income_based_csv_output: true,
  },
  physical_activity: {
    enabled: true,
    type: "simple",
    adjust_to_factors_mean: false,
    trended: false,
  },
  risk_factors: {
    adjust_to_factors_mean: true,
    trended: true,
  },
  trend: { enabled: false, type: "null" },
  two_stage: { use_logistic: false },
};

export function defaultRequirementsFromProject(
  project: ProjectDetail
): ProjectRequirementsState {
  const pr = project.default_project_requirements;
  if (!pr || Object.keys(pr).length === 0) {
    return JSON.parse(JSON.stringify(LEGACY_DEFAULT_REQUIREMENTS));
  }
  return {
    ...LEGACY_DEFAULT_REQUIREMENTS,
    ...JSON.parse(JSON.stringify(pr)),
    demographics: {
      ...LEGACY_DEFAULT_REQUIREMENTS.demographics,
      ...(pr.demographics as object | undefined),
    },
    income: {
      ...LEGACY_DEFAULT_REQUIREMENTS.income,
      ...(pr.income as object | undefined),
    },
    physical_activity: {
      ...LEGACY_DEFAULT_REQUIREMENTS.physical_activity,
      ...(pr.physical_activity as object | undefined),
    },
    risk_factors: {
      ...LEGACY_DEFAULT_REQUIREMENTS.risk_factors,
      ...(pr.risk_factors as object | undefined),
    },
    trend: {
      ...LEGACY_DEFAULT_REQUIREMENTS.trend,
      ...(pr.trend as object | undefined),
    },
    two_stage: {
      ...LEGACY_DEFAULT_REQUIREMENTS.two_stage,
      ...(pr.two_stage as object | undefined),
    },
  };
}

export function defaultRunSettings(project: ProjectDetail): RunSettings {
  const d = project.local_defaults;
  return {
    size_fraction: (d.size_fraction as number) ?? 0.0001,
    age_range_min: 0,
    age_range_max: 110,
    start_time: (d.start_time as number) ?? 2022,
    stop_time: (d.stop_time as number) ?? 2025,
    trial_runs: (d.trial_runs as number) ?? 1,
    active_intervention: (d.active_intervention as string) ?? "",
    thread_count: (d.thread_count as number) ?? 4,
    enabled_risk_factors: [...(project.model_risk_factors ?? [])],
  };
}
