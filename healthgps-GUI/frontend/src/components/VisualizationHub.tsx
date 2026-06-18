import { useEffect, useState } from "react";
import { api, type VisualizationBundle } from "../api/client";
import LabeledLineChart from "./LabeledLineChart";
import BurdenDeltaChart from "./viz/BurdenDeltaChart";
import ComorbidityMatrix from "./viz/ComorbidityMatrix";
import DumbbellChart from "./viz/DumbbellChart";
import HeadlineMetrics from "./viz/HeadlineMetrics";
import PipelineGraph from "./viz/PipelineGraph";
import VizPlaceholder from "./viz/VizPlaceholder";

type Tab = "construction" | "policy" | "equity" | "inspection" | "modelling";

interface Props {
  workspaceId: string;
  show: boolean;
  live?: boolean;
}

export default function VisualizationHub({ workspaceId, show, live = false }: Props) {
  const [tab, setTab] = useState<Tab>(show ? "policy" : "construction");
  const [data, setData] = useState<VisualizationBundle | null>(null);

  useEffect(() => {
    if (!workspaceId || (!show && !live)) return;
    const load = () => api.visualizations(workspaceId).then(setData).catch(() => {});
    load();
    if (!live) return;
    const id = window.setInterval(load, 2000);
    return () => window.clearInterval(id);
  }, [workspaceId, show, live]);

  const pipeline = data?.pipeline ?? null;

  if (!show && !live && !pipeline) return null;

  const tabs: { id: Tab; label: string }[] = [
    { id: "construction", label: "Construction" },
    { id: "policy", label: "Policy impact" },
    { id: "equity", label: "Equity" },
    { id: "inspection", label: "Inspection" },
    { id: "modelling", label: "Modelling" },
  ];

  return (
    <div className="viz-hub">
      <div className="viz-hub-header">
        <h3 className="grid-card-title">Visualisations</h3>
        {data?.meta?.result_file && (
          <span className="muted viz-hub-meta">{data.meta.result_file}</span>
        )}
      </div>

      {pipeline && (live || show) && (
        <div className="viz-hub-pipeline">
          <PipelineGraph modules={pipeline.modules} />
        </div>
      )}

      {show && (
        <>
          <div className="viz-tabs" role="tablist">
            {tabs.map((t) => (
              <button
                key={t.id}
                type="button"
                role="tab"
                aria-selected={tab === t.id}
                className={`viz-tab${tab === t.id ? " viz-tab--active" : ""}`}
                onClick={() => setTab(t.id)}
              >
                {t.label}
              </button>
            ))}
          </div>

          <div className="viz-panel">
            {tab === "construction" && (
              <div className="viz-panel-section">
                <p className="muted">{data?.scenario1?.validation_hint}</p>
                {pipeline && <PipelineGraph modules={pipeline.modules} />}
              </div>
            )}

            {tab === "policy" && data && (
              <div className="viz-panel-section">
                <HeadlineMetrics items={data.scenario2.headlines} />
                <BurdenDeltaChart bars={data.scenario2.burden_bars} />
                {data.scenario2.uncertainty_note && (
                  <p className="viz-note muted">{data.scenario2.uncertainty_note}</p>
                )}
                <div className="viz-charts-grid">
                  {data.scenario2.trajectories.map((chart) => (
                    <div key={chart.id} className="result-chart-card">
                      <LabeledLineChart
                        title={chart.title}
                        xLabel={chart.x_label}
                        yLabel={chart.y_label}
                        series={chart.series}
                        compact
                        formatX={(x) => String(Math.round(x))}
                      />
                    </div>
                  ))}
                </div>
              </div>
            )}

            {tab === "equity" && data && (
              <div className="viz-panel-section">
                <p className="viz-note muted">{data.scenario3.note}</p>
                <DumbbellChart items={data.scenario3.dumbbells} outcome={data.scenario3.outcome} />
                {data.scenario3.dumbbells.length === 0 && (
                  <VizPlaceholder
                    title="Stratified outcomes"
                    message="Enable income-based CSV output and re-run to see quintile dumbbells."
                  />
                )}
              </div>
            )}

            {tab === "inspection" && data && (
              <div className="viz-panel-section">
                <div className="repro-card">
                  <h4 className="viz-section-title">Reproducibility metadata</h4>
                  <dl className="repro-dl">
                    <dt>Model</dt>
                    <dd>{String(data.scenario4.reproducibility.model ?? "—")}</dd>
                    <dt>Version</dt>
                    <dd>{String(data.scenario4.reproducibility.version ?? "—")}</dd>
                    <dt>Seed</dt>
                    <dd>{String(data.scenario4.reproducibility.seed ?? "—")}</dd>
                    <dt>Intervention</dt>
                    <dd>{String(data.scenario4.reproducibility.intervention ?? "—")}</dd>
                  </dl>
                  <p className="muted">{String(data.scenario4.reproducibility.message ?? "")}</p>
                </div>
                <VizPlaceholder
                  title={data.scenario4.individual_tracking.title}
                  message={data.scenario4.individual_tracking.message}
                />
              </div>
            )}

            {tab === "modelling" && data && (
              <div className="viz-panel-section">
                {data.modelling.population_pyramid && (
                  <div className="pyramid-card">
                    <h4 className="viz-section-title">
                      Population snapshot ({data.modelling.population_pyramid.year})
                    </h4>
                    <div className="pyramid-bars">
                      <div className="pyramid-side">
                        <span>M {data.modelling.population_pyramid.male.toLocaleString()}</span>
                        <div className="pyramid-bar pyramid-bar--male" style={{ width: `${data.modelling.population_pyramid.male_pct}%` }} />
                      </div>
                      <div className="pyramid-side">
                        <span>F {data.modelling.population_pyramid.female.toLocaleString()}</span>
                        <div className="pyramid-bar pyramid-bar--female" style={{ width: `${data.modelling.population_pyramid.female_pct}%` }} />
                      </div>
                    </div>
                  </div>
                )}
                <ComorbidityMatrix matrix={data.modelling.comorbidity_matrix} />
                <div className="viz-charts-grid">
                  {data.modelling.risk_factor_trends.map((chart) => (
                    <div key={chart.id} className="result-chart-card">
                      <LabeledLineChart
                        title={chart.title}
                        xLabel={chart.x_label}
                        yLabel={chart.y_label}
                        series={chart.series}
                        compact
                      />
                    </div>
                  ))}
                </div>
                <VizPlaceholder title={data.modelling.calibration.title} message={data.modelling.calibration.message} />
                <VizPlaceholder title={data.modelling.convergence.title} message={data.modelling.convergence.message} />
                <VizPlaceholder title={data.modelling.tornado.title} message={data.modelling.tornado.message} />
                <VizPlaceholder title={data.modelling.sankey.title} message={data.modelling.sankey.message} />
              </div>
            )}

            {!data && show && <p className="muted">Loading visualisations…</p>}
          </div>
        </>
      )}
    </div>
  );
}
