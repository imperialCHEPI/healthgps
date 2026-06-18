import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import {
  api,
  defaultRequirementsFromProject,
  defaultRunSettings,
  LEGACY_DEFAULT_REQUIREMENTS,
  type CountryOption,
  type ProjectDetail,
  type ProjectRequirementsState,
  type RunSettings,
} from "../api/client";
import ProjectRequirementsPanel from "../components/ProjectRequirementsPanel";

export default function NewUserWizard() {
  const navigate = useNavigate();
  const [step, setStep] = useState<1 | 2 | 3>(1);
  const [countries, setCountries] = useState<CountryOption[]>([]);
  const [selectedCountry, setSelectedCountry] = useState<CountryOption | null>(
    null
  );
  const [requirements, setRequirements] =
    useState<ProjectRequirementsState>(LEGACY_DEFAULT_REQUIREMENTS);
  const [runSettings, setRunSettings] = useState<RunSettings | null>(null);
  const [modelRiskFactors, setModelRiskFactors] = useState<string[]>([]);
  const [populationLabel, setPopulationLabel] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api
      .countries()
      .then(setCountries)
      .catch((e: Error) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  const selectCountry = async (country: CountryOption) => {
    setSelectedCountry(country);
    setPopulationLabel(`${country.name} virtual population`);
    setError(null);
    try {
      const defaults = await api.newUserDefaults(country.id);
      const pseudoProject = {
        id: defaults.project_id,
        name: defaults.project_name,
        description: "",
        has_pif: false,
        locked_fields: [],
        examples_root: "",
        example_dir_path: "",
        default_config_variant: "config",
        default_config_path: "",
        config_options: [],
        default_project_requirements: defaults.default_project_requirements,
        model_risk_factors: defaults.model_risk_factors,
        local_defaults: defaults.local_defaults,
        intervention_ids: [""],
      } as ProjectDetail;
      setRequirements(defaultRequirementsFromProject(pseudoProject));
      setRunSettings(defaultRunSettings(pseudoProject));
      setModelRiskFactors(defaults.model_risk_factors ?? []);
      setStep(2);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
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

  const createPopulation = async () => {
    if (!selectedCountry || !runSettings) return;
    setBusy(true);
    setError(null);
    try {
      const meta = await api.createNewUserSession({
        country_id: selectedCountry.id,
        country_name: selectedCountry.name,
        population_label: populationLabel,
        project_requirements: requirements,
        run_settings: runSettings,
      });
      navigate(`/workspace/${meta.id}`);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  if (loading) {
    return (
      <div className="wizard-page wizard-page--centered">
        <p className="muted">Loading countries…</p>
      </div>
    );
  }

  return (
    <div
      className={`wizard-page${step === 2 ? "" : " wizard-page--centered"}`}
    >
      <div className="wizard-header">
        <button type="button" className="secondary" onClick={() => navigate("/")}>
          ← Home
        </button>
        <div>
          <h2>New user — build a virtual population</h2>
          <p className="muted">
            Step {step} of 3 — not tied to a single programme; configure your
            own baseline population.
          </p>
        </div>
      </div>

      {error && <div className="alert alert-error">{error}</div>}

      {step === 1 && (
        <section className="wizard-section">
          <h3 className="wizard-section-title">Choose your country</h3>
          <div className="country-select-grid">
            {countries.map((c) => (
              <button
                key={c.id}
                type="button"
                className={`country-select-card${c.has_example_data ? "" : " country-select-card--planned"}`}
                onClick={() => selectCountry(c)}
              >
                <strong>{c.name}</strong>
                <span className="muted">
                  {c.has_example_data
                    ? "Example demographics available"
                    : "Template defaults (no local data yet)"}
                </span>
              </button>
            ))}
          </div>
        </section>
      )}

      {step === 2 && selectedCountry && runSettings && (
        <section className="wizard-section">
          <div className="wizard-step-bar">
            <h3 className="wizard-section-title">
              Configure population — {selectedCountry.name}
            </h3>
            <button type="button" className="secondary" onClick={() => setStep(1)}>
              Change country
            </button>
          </div>

          <div className="field" style={{ maxWidth: "24rem", marginBottom: "1rem" }}>
            <label>Population label</label>
            <input
              type="text"
              value={populationLabel}
              onChange={(e) => setPopulationLabel(e.target.value)}
            />
          </div>

          <div className="wizard-config-bar">
            <div className="field field--compact">
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
            <div className="field field--compact">
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
            <div className="field field--compact">
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
          </div>

          <div className="workspace-dashboard wizard-modules">
            <ProjectRequirementsPanel
              value={requirements}
              lockedFields={[]}
              hasPif={false}
              pifEnabled={false}
              modelRiskFactors={modelRiskFactors}
              enabledRiskFactors={runSettings.enabled_risk_factors ?? []}
              onChange={setRequirements}
              onPifChange={() => {}}
              onRiskFactorToggle={toggleRiskFactor}
              onEnabledRiskFactorsChange={(factors) =>
                setRunSettings({ ...runSettings, enabled_risk_factors: factors })
              }
            />
          </div>

          <div className="wizard-actions">
            <button type="button" className="primary" onClick={() => setStep(3)}>
              Review &amp; create
            </button>
          </div>
        </section>
      )}

      {step === 3 && selectedCountry && runSettings && (
        <section className="wizard-section">
          <h3 className="wizard-section-title">Create virtual population</h3>
          <div className="wizard-summary">
            <p>
              <strong>Country:</strong> {selectedCountry.name}
            </p>
            <p>
              <strong>Label:</strong> {populationLabel}
            </p>
            <p>
              <strong>Population size:</strong>{" "}
              {(runSettings.size_fraction * 100).toFixed(4)}% of reference
            </p>
            <p>
              <strong>Risk factors:</strong>{" "}
              {(runSettings.enabled_risk_factors ?? []).length} selected
            </p>
            <p className="muted">
              A baseline virtual population will be initialised with your chosen
              demographics, socioeconomic, and risk-factor settings. You can
              validate and run the simulation from the workspace.
            </p>
          </div>
          <div className="wizard-actions">
            <button type="button" className="secondary" onClick={() => setStep(2)}>
              Back
            </button>
            <button
              type="button"
              className="primary"
              disabled={busy}
              onClick={createPopulation}
            >
              {busy ? "Creating…" : "Create population"}
            </button>
          </div>
        </section>
      )}
    </div>
  );
}
