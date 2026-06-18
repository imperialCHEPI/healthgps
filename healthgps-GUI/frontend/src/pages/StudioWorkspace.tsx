import { useCallback, useEffect, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import {
  api,
  defaultRequirementsFromProject,
  defaultRunSettings,
  type ProjectDetail,
  type WorkspaceMeta,
  type ProjectRequirementsState,
  type RunSettings,
} from "../api/client";
import ConsentModal from "../components/ConsentModal";
import ProjectRequirementsPanel from "../components/ProjectRequirementsPanel";
import SimulationDashboard from "../components/SimulationDashboard";
import RunMonitor from "./RunMonitor";

const CONSENT_KEY = "healthgps-studio-consent-session";

export default function StudioWorkspace() {
  const { projectId, workspaceId } = useParams<{
    projectId?: string;
    workspaceId?: string;
  }>();
  const navigate = useNavigate();
  const isNew = Boolean(projectId);

  const [project, setProject] = useState<ProjectDetail | null>(null);
  const [requirements, setRequirements] =
    useState<ProjectRequirementsState | null>(null);
  const [runSettings, setRunSettings] = useState<RunSettings | null>(null);
  const [pifEnabled, setPifEnabled] = useState(true);
  const [wsId, setWsId] = useState<string | null>(workspaceId ?? null);
  const [error, setError] = useState<string | null>(null);
  const [schemaErrors, setSchemaErrors] = useState<string[]>([]);
  const [consentOpen, setConsentOpen] = useState(false);
  const [consentAction, setConsentAction] = useState<"validate" | "run">(
    "validate"
  );
  const [previewCommand, setPreviewCommand] = useState("");
  const [polling, setPolling] = useState(false);
  const [simActive, setSimActive] = useState(false);
  const [busy, setBusy] = useState(false);
  const [noConsole, setNoConsole] = useState(false);
  const [configVariant, setConfigVariant] = useState("config");
  const [sessionMeta, setSessionMeta] = useState<WorkspaceMeta | null>(null);

  const loadExpertWorkspace = (meta: WorkspaceMeta) => {
    const expertProject: ProjectDetail = {
      id: "expert",
      name: meta.session_label || meta.country_name || "Expert session",
      description: "Uploaded configuration",
      has_pif: false,
      locked_fields: [],
      examples_root: "",
      example_dir_path: meta.active_config_path,
      default_config_variant: "uploaded",
      default_config_path: meta.active_config_path,
      config_options: [
        {
          id: "uploaded",
          label: "Uploaded config",
          file: "config.json",
          path: meta.active_config_path,
          exists: true,
        },
      ],
      default_project_requirements: meta.project_requirements,
      model_risk_factors: [],
      local_defaults: {},
      intervention_ids: [""],
    };
    setProject(expertProject);
    setRequirements(
      defaultRequirementsFromProject({
        ...expertProject,
        default_project_requirements: meta.project_requirements,
      })
    );
    const saved = meta.run_settings as RunSettings;
    setRunSettings({
      ...defaultRunSettings(expertProject),
      ...saved,
      enabled_risk_factors: saved.enabled_risk_factors ?? [],
    });
  };

  useEffect(() => {
    api.getSettings().then((s) => setNoConsole(!s.healthgps_console));
  }, []);

  useEffect(() => {
    if (!projectId || !isNew) return;
    api
      .getProject(projectId, configVariant)
      .then((p) => {
        setProject(p);
        setRequirements(defaultRequirementsFromProject(p));
        setRunSettings(defaultRunSettings(p));
      })
      .catch((e: Error) => setError(e.message));
  }, [projectId, configVariant, isNew]);

  useEffect(() => {
    if (!workspaceId || isNew) return;
    api
      .getWorkspace(workspaceId)
      .then((meta) => {
        setWsId(meta.id);
        setSessionMeta(meta);
        setConfigVariant(meta.config_variant);
        if (meta.project_id === "expert") {
          loadExpertWorkspace(meta);
          return;
        }
        return api.getProject(meta.project_id, meta.config_variant).then((p) => {
          setProject(p);
          setRequirements(
            defaultRequirementsFromProject({
              ...p,
              default_project_requirements: meta.project_requirements,
            })
          );
          const saved = meta.run_settings as RunSettings;
          setRunSettings({
            ...defaultRunSettings(p),
            ...saved,
            enabled_risk_factors:
              saved.enabled_risk_factors?.length > 0
                ? saved.enabled_risk_factors
                : [...(p.model_risk_factors ?? [])],
          });
        });
      })
      .catch((e: Error) => setError(e.message));
  }, [workspaceId, isNew]);

  const persistWorkspace = useCallback(async () => {
    if (!project || !requirements || !runSettings) return null;
    const body = {
      project_id: project.id,
      config_variant: configVariant,
      project_requirements: requirements,
      run_settings: runSettings,
      pif_enabled: project.has_pif ? pifEnabled : null,
    };
    if (wsId) {
      await api.updateWorkspace(wsId, body);
      return wsId;
    }
    const meta = await api.createWorkspace(body);
    setWsId(meta.id);
    navigate(`/workspace/${meta.id}`, { replace: true });
    return meta.id;
  }, [project, configVariant, requirements, runSettings, pifEnabled, wsId, navigate]);

  const openConsent = async (action: "validate" | "run") => {
    setError(null);
    setSchemaErrors([]);
    setBusy(true);
    try {
      const id = await persistWorkspace();
      if (!id) return;
      const dry = action === "validate";
      const { command } = await api.previewCommand(id, dry);
      setPreviewCommand(command);
      if (sessionStorage.getItem(CONSENT_KEY) === "1") {
        await executeAction(action, id, true);
        return;
      }
      setConsentAction(action);
      setConsentOpen(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const executeAction = async (
    action: "validate" | "run",
    id: string,
    consent: boolean,
    skipSession = false
  ) => {
    setConsentOpen(false);
    if (skipSession) sessionStorage.setItem(CONSENT_KEY, "1");
    setBusy(true);
    setError(null);
    try {
      if (action === "validate") {
        const schema = await api.validateSchema(id);
        if (!schema.valid) {
          setSchemaErrors(schema.errors);
          return;
        }
        await api.validate(id, consent);
      } else {
        await api.run(id, consent);
      }
      setSimActive(true);
      setPolling(true);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  const toggleRiskFactor = (name: string, checked: boolean) => {
    if (!runSettings) return;
    const current = runSettings.enabled_risk_factors ?? [];
    const next = checked
      ? [...current, name]
      : current.filter((f) => f !== name);
    setRunSettings({ ...runSettings, enabled_risk_factors: next });
  };

  if (!project || !requirements || !runSettings) {
    return <p className="muted">Loading workspace…</p>;
  }

  return (
    <div className="workspace-page">
      <div className="workspace-toolbar workspace-toolbar--main">
        <button
          type="button"
          className="secondary"
          onClick={() => {
            if (sessionMeta?.session_type === "new_user") navigate("/new-user");
            else if (sessionMeta?.session_type === "expert") navigate("/expert");
            else navigate("/examples");
          }}
        >
          ← Back
        </button>
        <div className="workspace-title-row">
          <div className="workspace-title-block">
            <h2 className="workspace-title">
              {sessionMeta?.population_label ||
                sessionMeta?.session_label ||
                project.name}
              {sessionMeta?.country_name && sessionMeta.session_type === "new_user"
                ? ` (${sessionMeta.country_name})`
                : ""}
            </h2>
            <span className="muted workspace-path">{project.example_dir_path}</span>
          </div>
          <div className="workspace-toolbar-actions">
            <button
              type="button"
              className="secondary"
              disabled={busy}
              onClick={() => openConsent("validate")}
            >
              Validate
            </button>
            <button
              type="button"
              className="primary"
              disabled={busy || noConsole}
              onClick={() => openConsent("run")}
            >
              Run
            </button>
          </div>
        </div>
      </div>

      {noConsole && (
        <div className="alert alert-warning">
          HEALTHGPS_CONSOLE is not set on the backend.
        </div>
      )}
      {error && <div className="alert alert-error">{error}</div>}
      {schemaErrors.length > 0 && (
        <div className="alert alert-error">
          <strong>Schema validation failed:</strong>
          <ul>
            {schemaErrors.map((e) => (
              <li key={e}>{e}</li>
            ))}
          </ul>
        </div>
      )}

      <div className="workspace-split">
        <aside className="workspace-sidebar">
          <div className="sidebar-card">
            <h3 className="grid-card-title">Setup</h3>
            <div className="sidebar-fields">
              <div className="field">
                <label>Config (-c)</label>
                <select
                  value={configVariant}
                  onChange={(e) => setConfigVariant(e.target.value)}
                >
                  {project.config_options
                    .filter((o) => o.exists)
                    .map((o) => (
                      <option key={o.id} value={o.id}>
                        {o.label}
                      </option>
                    ))}
                </select>
              </div>
              <div className="field">
                <label>Intervention / policy</label>
                <select
                  value={runSettings.active_intervention}
                  onChange={(e) =>
                    setRunSettings({
                      ...runSettings,
                      active_intervention: e.target.value,
                    })
                  }
                >
                  {project.intervention_ids.map((id) => (
                    <option key={id || "baseline"} value={id}>
                      {id || "baseline"}
                    </option>
                  ))}
                </select>
              </div>
            </div>
          </div>

          <div className="sidebar-card">
            <h3 className="grid-card-title">Run parameters</h3>
            <div className="sidebar-fields sidebar-fields--grid">
              <div className="field">
                <label>Population %</label>
                <input
                  type="number"
                  min={0.00001}
                  max={1}
                  step={0.00001}
                  value={runSettings.size_fraction}
                  onChange={(e) =>
                    setRunSettings({
                      ...runSettings,
                      size_fraction: Number(e.target.value),
                    })
                  }
                />
              </div>
              <div className="field">
                <label>Threads (-T)</label>
                <input
                  type="number"
                  min={1}
                  value={runSettings.thread_count}
                  onChange={(e) =>
                    setRunSettings({
                      ...runSettings,
                      thread_count: Number(e.target.value),
                    })
                  }
                />
              </div>
              <div className="field">
                <label>Start year</label>
                <input
                  type="number"
                  value={runSettings.start_time}
                  onChange={(e) =>
                    setRunSettings({
                      ...runSettings,
                      start_time: Number(e.target.value),
                    })
                  }
                />
              </div>
              <div className="field">
                <label>Stop year</label>
                <input
                  type="number"
                  value={runSettings.stop_time}
                  onChange={(e) =>
                    setRunSettings({
                      ...runSettings,
                      stop_time: Number(e.target.value),
                    })
                  }
                />
              </div>
              <div className="field">
                <label>Trial runs</label>
                <input
                  type="number"
                  min={1}
                  value={runSettings.trial_runs}
                  onChange={(e) =>
                    setRunSettings({
                      ...runSettings,
                      trial_runs: Number(e.target.value),
                    })
                  }
                />
              </div>
            </div>
          </div>

          <ProjectRequirementsPanel
            variant="sidebar"
            value={requirements}
            lockedFields={project.locked_fields}
            hasPif={project.has_pif}
            pifEnabled={pifEnabled}
            modelRiskFactors={project.model_risk_factors ?? []}
            enabledRiskFactors={runSettings.enabled_risk_factors ?? []}
            onChange={setRequirements}
            onPifChange={setPifEnabled}
            onRiskFactorToggle={toggleRiskFactor}
            onEnabledRiskFactorsChange={(factors) =>
              setRunSettings({ ...runSettings, enabled_risk_factors: factors })
            }
          />
        </aside>

        <section className="workspace-live">
          <SimulationDashboard
            workspaceId={wsId}
            polling={polling}
            active={simActive}
          />
          {wsId && (
            <RunMonitor
              workspaceId={wsId}
              polling={polling}
              onPollingChange={setPolling}
            />
          )}
        </section>
      </div>

      <ConsentModal
        open={consentOpen}
        command={previewCommand}
        actionLabel={consentAction === "validate" ? "Validate" : "Run"}
        onCancel={() => setConsentOpen(false)}
        onConfirm={(skip) => {
          if (!wsId) return;
          executeAction(consentAction, wsId, true, skip);
        }}
      />
    </div>
  );
}
