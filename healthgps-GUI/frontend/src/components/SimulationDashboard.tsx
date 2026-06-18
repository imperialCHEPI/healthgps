import { useEffect, useState } from "react";
import { api, type RunTelemetry } from "../api/client";
import ResourceMonitorChart from "./ResourceMonitorChart";
import ResultLineCharts from "./ResultLineCharts";

const IDLE_TELEMETRY: RunTelemetry = {
  state: "idle",
  phase: "idle",
  phase_message: "Run or Validate to see live simulation metrics here.",
  current_year: null,
  start_year: 2022,
  stop_year: 2025,
  year_progress_pct: 0,
  population_initialized: 0,
  target_population: 0,
  policy_label: "—",
  cpu_percent: 0,
  memory_mb: 0,
  cpu_history: [],
  memory_history: [],
  gender_male_pct: 50,
  gender_female_pct: 50,
  age_bins: [],
  enabled_attributes: [],
  events: [],
  phase_steps: [],
  dry_run: false,
};

interface Props {
  workspaceId: string | null;
  polling: boolean;
  active: boolean;
}

export default function SimulationDashboard({
  workspaceId,
  polling,
  active,
}: Props) {
  const [telemetry, setTelemetry] = useState<RunTelemetry>(IDLE_TELEMETRY);

  useEffect(() => {
    if (!workspaceId) return;
    api.runTelemetry(workspaceId).then(setTelemetry).catch(() => {});
  }, [workspaceId]);

  useEffect(() => {
    if (!workspaceId || !polling) return;
    const tick = () => api.runTelemetry(workspaceId).then(setTelemetry).catch(() => {});
    tick();
    const id = window.setInterval(tick, 1000);
    return () => window.clearInterval(id);
  }, [workspaceId, polling]);

  const t = active ? telemetry : IDLE_TELEMETRY;
  const maxAge = Math.max(...t.age_bins.map((b) => b.count), 1);
  const showResults = t.phase === "complete" && Boolean(workspaceId);
  const hasResourceChart =
    t.cpu_history.length > 1 ||
    t.memory_history.length > 1 ||
    t.cpu_percent > 0 ||
    t.memory_mb > 0;

  return (
    <div className={`sim-dashboard${active ? " sim-dashboard--live" : ""}`}>
      <div className="sim-dashboard-header">
        <div className="sim-dashboard-header-main">
          <h3 className="grid-card-title">Live simulation</h3>
          <p className="sim-phase-message">{t.phase_message}</p>
        </div>
        <span className={`sim-phase-badge sim-phase-badge--${t.phase}`}>
          {t.phase.replace(/_/g, " ")}
        </span>
      </div>

      <div className="sim-dashboard-body">
        <div className="sim-dashboard-primary">
          {t.phase_steps.length > 0 && (
            <div className="sim-phase-steps sim-phase-steps--compact">
              {t.phase_steps.map((step) => (
                <div
                  key={step.id}
                  className={`sim-phase-step sim-phase-step--compact sim-phase-step--${step.status}`}
                  title={`${step.label}: ${step.progress_pct}%`}
                >
                  <span className="sim-phase-step-label">{step.label}</span>
                  <div className="sim-progress-track sim-progress-track--thin">
                    <div
                      className="sim-progress-fill"
                      style={{ width: `${step.progress_pct}%` }}
                    />
                  </div>
                  <span className="sim-phase-step-pct">{step.progress_pct}%</span>
                </div>
              ))}
            </div>
          )}

          <div className="sim-kpi-row">
            <div className="sim-kpi">
              <span className="sim-kpi-label">Population</span>
              <strong>
                {t.population_initialized.toLocaleString()}
                {t.target_population > 0 && (
                  <span className="muted">
                    /{t.target_population.toLocaleString()}
                  </span>
                )}
              </strong>
            </div>
            <div className="sim-kpi">
              <span className="sim-kpi-label">Policy</span>
              <strong>{t.policy_label}</strong>
            </div>
            <div className="sim-kpi sim-kpi--wide">
              <span className="sim-kpi-label">
                Years {t.start_year}–{t.stop_year}
                {t.current_year != null ? ` · ${t.current_year}` : ""}
              </span>
              <div className="sim-progress-track sim-progress-track--thin">
                <div
                  className="sim-progress-fill"
                  style={{ width: `${t.year_progress_pct}%` }}
                />
              </div>
            </div>
          </div>

          <div className="sim-charts-row sim-charts-row--compact">
            <div className="sim-chart-card sim-chart-card--compact">
              <h4 className="sim-chart-title">Gender</h4>
              <div className="sim-bar-pair">
                <div className="sim-bar-row">
                  <span>M</span>
                  <div className="sim-bar-track">
                    <div
                      className="sim-bar-fill sim-bar-fill--male"
                      style={{ width: `${t.gender_male_pct}%` }}
                    />
                  </div>
                  <span>{t.gender_male_pct.toFixed(0)}%</span>
                </div>
                <div className="sim-bar-row">
                  <span>F</span>
                  <div className="sim-bar-track">
                    <div
                      className="sim-bar-fill sim-bar-fill--female"
                      style={{ width: `${t.gender_female_pct}%` }}
                    />
                  </div>
                  <span>{t.gender_female_pct.toFixed(0)}%</span>
                </div>
              </div>
            </div>

            <div className="sim-chart-card sim-chart-card--compact">
              <h4 className="sim-chart-title">Age</h4>
              {t.age_bins.length === 0 ? (
                <p className="muted sim-chart-empty">Enable age</p>
              ) : (
                <div className="sim-age-bars sim-age-bars--compact">
                  {t.age_bins.slice(0, 4).map((bin) => (
                    <div key={bin.label} className="sim-age-row">
                      <span className="sim-age-label">{bin.label}</span>
                      <div className="sim-bar-track">
                        <div
                          className="sim-bar-fill sim-bar-fill--age"
                          style={{ width: `${(bin.count / maxAge) * 100}%` }}
                        />
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        </div>

        {hasResourceChart && (
          <div className="sim-dashboard-aside">
            <ResourceMonitorChart
              cpuHistory={t.cpu_history}
              memoryHistory={t.memory_history}
              cpuCurrent={t.cpu_percent}
              memoryCurrent={t.memory_mb}
            />
          </div>
        )}
      </div>

      {workspaceId && (
        <ResultLineCharts workspaceId={workspaceId} show={showResults} />
      )}

      {t.events.length > 0 && (
        <ul className="sim-event-feed sim-event-feed--compact">
          {t.events.slice(-3).map((ev, i) => (
            <li key={`${ev}-${i}`}>{ev}</li>
          ))}
        </ul>
      )}
    </div>
  );
}
