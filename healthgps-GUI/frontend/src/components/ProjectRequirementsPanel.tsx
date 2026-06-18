import type { ProjectRequirementsState } from "../api/client";

interface Props {
  value: ProjectRequirementsState;
  lockedFields: string[];
  hasPif: boolean;
  pifEnabled: boolean;
  modelRiskFactors: string[];
  enabledRiskFactors: string[];
  onChange: (next: ProjectRequirementsState) => void;
  onPifChange: (enabled: boolean) => void;
  onRiskFactorToggle: (name: string, checked: boolean) => void;
  onEnabledRiskFactorsChange: (factors: string[]) => void;
  variant?: "grid" | "sidebar";
}

function isLocked(locked: string[], path: string) {
  return locked.includes(path);
}

function Toggle({
  label,
  checked,
  disabled,
  title,
  onChange,
}: {
  label: string;
  checked: boolean;
  disabled?: boolean;
  title?: string;
  onChange: (v: boolean) => void;
}) {
  return (
    <div className={`field toggle-row ${disabled ? "locked" : ""}`} title={title}>
      <label>{label}</label>
      <label className="toggle">
        <input
          type="checkbox"
          checked={checked}
          disabled={disabled}
          onChange={(e) => onChange(e.target.checked)}
        />
        <span className="toggle-slider" />
      </label>
    </div>
  );
}

export default function ProjectRequirementsPanel({
  value,
  lockedFields,
  hasPif,
  pifEnabled,
  modelRiskFactors,
  enabledRiskFactors,
  onChange,
  onPifChange,
  onRiskFactorToggle,
  onEnabledRiskFactorsChange,
  variant = "grid",
}: Props) {
  const cardClass =
    variant === "sidebar" ? "sidebar-card" : "grid-card grid-card--module";
  const riskClass =
    variant === "sidebar" ? "sidebar-card" : "grid-card grid-card--risk-factors";
  const setDemo = (patch: Partial<ProjectRequirementsState["demographics"]>) =>
    onChange({
      ...value,
      demographics: { ...value.demographics, ...patch },
    });
  const setIncome = (patch: Partial<ProjectRequirementsState["income"]>) =>
    onChange({ ...value, income: { ...value.income, ...patch } });
  const setPa = (patch: Partial<ProjectRequirementsState["physical_activity"]>) =>
    onChange({
      ...value,
      physical_activity: { ...value.physical_activity, ...patch },
    });
  const setRf = (patch: Partial<ProjectRequirementsState["risk_factors"]>) =>
    onChange({ ...value, risk_factors: { ...value.risk_factors, ...patch } });
  const setTrend = (patch: Partial<ProjectRequirementsState["trend"]>) =>
    onChange({ ...value, trend: { ...value.trend, ...patch } });
  const setTwo = (patch: Partial<ProjectRequirementsState["two_stage"]>) =>
    onChange({ ...value, two_stage: { ...value.two_stage, ...patch } });

  const lockTip = (path: string) =>
    isLocked(lockedFields, path) ? "Locked for this project template" : undefined;

  return (
    <>
      <div className={cardClass}>
        <h3 className="grid-card-title">Demographics</h3>
        <div className="grid-card-body grid-card-body--toggles">
          <Toggle
            label="Age"
            checked={value.demographics.age}
            disabled={isLocked(lockedFields, "demographics.age")}
            title={lockTip("demographics.age")}
            onChange={(v) => setDemo({ age: v })}
          />
          <Toggle
            label="Gender"
            checked={value.demographics.gender}
            disabled={isLocked(lockedFields, "demographics.gender")}
            title={lockTip("demographics.gender")}
            onChange={(v) => setDemo({ gender: v })}
          />
          <Toggle
            label="Region"
            checked={value.demographics.region}
            disabled={isLocked(lockedFields, "demographics.region")}
            title={lockTip("demographics.region")}
            onChange={(v) => setDemo({ region: v })}
          />
          <Toggle
            label="Ethnicity"
            checked={value.demographics.ethnicity}
            disabled={isLocked(lockedFields, "demographics.ethnicity")}
            title={lockTip("demographics.ethnicity")}
            onChange={(v) => setDemo({ ethnicity: v })}
          />
          {value.demographics.age && (
            <div className="field field--span2">
              <label>Max age for linear models</label>
              <input
                type="number"
                min={1}
                value={value.demographics.max_age_for_linear_models ?? ""}
                placeholder="optional"
                onChange={(e) =>
                  setDemo({
                    max_age_for_linear_models: e.target.value
                      ? Number(e.target.value)
                      : null,
                  })
                }
              />
            </div>
          )}
        </div>
      </div>

      <div className={cardClass}>
        <h3 className="grid-card-title">Socioeconomic</h3>
        <div className="grid-card-body grid-card-body--toggles">
          <Toggle
            label="Enabled"
            checked={value.income.enabled}
            onChange={(v) => setIncome({ enabled: v })}
          />
          <Toggle
            label="Adjust to factors mean"
            checked={value.income.adjust_to_factors_mean}
            onChange={(v) => setIncome({ adjust_to_factors_mean: v })}
          />
          <Toggle
            label="Trended"
            checked={value.income.trended}
            onChange={(v) => setIncome({ trended: v })}
          />
          <Toggle
            label="Income-based CSV output"
            checked={value.income.income_based_csv_output}
            onChange={(v) => setIncome({ income_based_csv_output: v })}
          />
          <div className="field">
            <label>Type</label>
            <select
              value={value.income.type}
              disabled={isLocked(lockedFields, "income.type")}
              title={lockTip("income.type")}
              onChange={(e) => setIncome({ type: e.target.value })}
            >
              <option value="continuous">continuous</option>
              <option value="categorical">categorical</option>
            </select>
          </div>
          <div className="field">
            <label>Categories</label>
            <select
              value={value.income.categories}
              onChange={(e) => setIncome({ categories: e.target.value })}
            >
              <option value="3">3</option>
              <option value="4">4</option>
              <option value="5">5</option>
            </select>
          </div>
        </div>
      </div>

      <div className={cardClass}>
        <h3 className="grid-card-title">Physical activity</h3>
        <div className="grid-card-body grid-card-body--toggles">
          <Toggle
            label="Enabled"
            checked={value.physical_activity.enabled}
            onChange={(v) => setPa({ enabled: v })}
          />
          <Toggle
            label="Adjust to factors mean"
            checked={value.physical_activity.adjust_to_factors_mean}
            onChange={(v) => setPa({ adjust_to_factors_mean: v })}
          />
          <Toggle
            label="Trended"
            checked={value.physical_activity.trended}
            onChange={(v) => setPa({ trended: v })}
          />
          <div className="field">
            <label>Type</label>
            <select
              value={value.physical_activity.type}
              disabled={isLocked(lockedFields, "physical_activity.type")}
              title={lockTip("physical_activity.type")}
              onChange={(e) => setPa({ type: e.target.value })}
            >
              <option value="simple">simple</option>
              <option value="continuous">continuous</option>
            </select>
          </div>
        </div>
      </div>

      <div className={cardClass}>
        <h3 className="grid-card-title">Trend &amp; model</h3>
        <div className="grid-card-body grid-card-body--toggles">
          <Toggle
            label="Trend enabled"
            checked={value.trend.enabled}
            onChange={(v) => setTrend({ enabled: v })}
          />
          <Toggle
            label="Use logistic (two-stage)"
            checked={value.two_stage.use_logistic}
            onChange={(v) => setTwo({ use_logistic: v })}
          />
          <div className="field field--span2">
            <label>Trend type</label>
            <select
              value={value.trend.type}
              onChange={(e) => setTrend({ type: e.target.value })}
            >
              <option value="null">null</option>
              <option value="trend">trend</option>
              <option value="upf_trend">upf_trend</option>
              <option value="UPFTrend">UPFTrend</option>
              <option value="income_trend">income_trend</option>
            </select>
          </div>
        </div>
      </div>

      <div className={riskClass}>
        <div className="risk-factors-header">
          <h3 className="grid-card-title grid-card-title--inline">Risk factors</h3>
          <div className="risk-factors-header-toggles">
            <Toggle
              label="Adjust to mean"
              checked={value.risk_factors.adjust_to_factors_mean}
              onChange={(v) => setRf({ adjust_to_factors_mean: v })}
            />
            <Toggle
              label="Trended"
              checked={value.risk_factors.trended}
              onChange={(v) => setRf({ trended: v })}
            />
          </div>
          {modelRiskFactors.length > 0 && (
            <div className="risk-factor-bulk">
              <button
                type="button"
                className="link-btn"
                onClick={() => onEnabledRiskFactorsChange([...modelRiskFactors])}
              >
                All
              </button>
              <span className="risk-factor-bulk-sep">·</span>
              <button
                type="button"
                className="link-btn"
                onClick={() => onEnabledRiskFactorsChange([])}
              >
                None
              </button>
              <span className="risk-factor-count muted">
                {enabledRiskFactors.length}/{modelRiskFactors.length} selected
              </span>
            </div>
          )}
        </div>
        {modelRiskFactors.length > 0 && (
          <div className="risk-factor-grid">
            {modelRiskFactors.map((name) => (
              <label
                key={name}
                className={`risk-factor-chip${enabledRiskFactors.includes(name) ? " risk-factor-chip--on" : ""}`}
              >
                <input
                  type="checkbox"
                  checked={enabledRiskFactors.includes(name)}
                  onChange={(e) => onRiskFactorToggle(name, e.target.checked)}
                />
                <span>{name}</span>
              </label>
            ))}
          </div>
        )}
      </div>

      {hasPif && (
        <div className={cardClass}>
          <h3 className="grid-card-title">Population Impact Fraction</h3>
          <Toggle
            label="PIF enabled"
            checked={pifEnabled}
            onChange={onPifChange}
          />
        </div>
      )}
    </>
  );
}
