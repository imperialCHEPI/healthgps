import { useEffect, useState } from "react";
import { api, type VisualizationBundle } from "../api/client";
import ChartExplorer from "./ChartExplorer";
import FlexibleChart, { type ChartType } from "./FlexibleChart";
import BurdenDeltaChart from "./viz/BurdenDeltaChart";
import ComorbidityMatrix from "./viz/ComorbidityMatrix";
import HeadlineMetrics from "./viz/HeadlineMetrics";
import PipelineGraph from "./viz/PipelineGraph";

const DEFAULT_CHART_TYPES: { id: ChartType; label: string }[] = [
  { id: "line", label: "Line" },
  { id: "area", label: "Area" },
  { id: "smooth", label: "Smooth line" },
  { id: "step", label: "Step line" },
  { id: "bar", label: "Bar" },
  { id: "column", label: "Column" },
  { id: "stacked_bar", label: "Stacked bar" },
  { id: "scatter", label: "Scatter" },
  { id: "pie", label: "Pie (latest year)" },
  { id: "combo", label: "Combo (bar + line)" },
];

interface Props {
  workspaceId: string;
  show: boolean;
  live?: boolean;
  generating?: boolean;
  onReady?: () => void;
  livePipeline?: VisualizationBundle["pipeline"] | null;
}

export default function VisualizationHub({
  workspaceId,
  show,
  live = false,
  generating = false,
  onReady,
  livePipeline = null,
}: Props) {
  const [data, setData] = useState<VisualizationBundle | null>(null);

  const pipeline = livePipeline ?? data?.pipeline ?? null;
  const chartBuilder = data?.chart_builder;
  const hasResultData = Boolean(chartBuilder?.variables?.length);
  const showCharts = show || hasResultData;
  const scenario2 = data?.scenario2;
  const autoCharts = scenario2?.charts ?? [];
  const trajectories = scenario2?.trajectories ?? [];

  useEffect(() => {
    if (!workspaceId || (!show && !live && !generating)) return;
    const load = () =>
      api
        .visualizations(workspaceId)
        .then((bundle) => {
          setData(bundle);
          if (show || bundle.chart_builder?.variables?.length) onReady?.();
        })
        .catch(() => {});
    load();
    if (!live && !generating && !show) return;
    const id = window.setInterval(load, generating ? 1500 : 2000);
    return () => window.clearInterval(id);
  }, [workspaceId, show, live, generating, onReady]);

  if (!show && !live && !pipeline) return null;

  const chartTypes = (chartBuilder?.chart_types ?? DEFAULT_CHART_TYPES) as {
    id: ChartType;
    label: string;
  }[];

  return (
    <div className="viz-hub">
      <div className="viz-hub-header">
        <h3 className="grid-card-title">Results &amp; charts</h3>
        {(data?.meta?.result_file || chartBuilder?.result_file) && (
          <span className="muted viz-hub-meta">
            Source: {data?.meta?.result_file ?? chartBuilder?.result_file}
          </span>
        )}
      </div>

      {generating && !hasResultData && (
        <div className="viz-generating" role="status">
          <div className="viz-generating-spinner" aria-hidden />
          <p>Loading variables from your result JSON…</p>
        </div>
      )}

      {live && !showCharts && pipeline && (
        <div className="viz-hub-pipeline">
          <p className="muted viz-live-note">Simulation pipeline (live)</p>
          <PipelineGraph modules={pipeline.modules} />
        </div>
      )}

      {showCharts && data && (
        <>
          {scenario2?.headlines && scenario2.headlines.length > 0 && (
            <div className="viz-panel-section">
              <h4 className="viz-section-title">Policy impact (from result JSON)</h4>
              <HeadlineMetrics items={scenario2.headlines} />
            </div>
          )}

          {scenario2?.burden_bars && scenario2.burden_bars.length > 0 && (
            <div className="viz-panel-section">
              <BurdenDeltaChart bars={scenario2.burden_bars} />
            </div>
          )}

          {(autoCharts.length > 0 || trajectories.length > 0) && (
            <div className="viz-panel-section">
              <h4 className="viz-section-title">Charts from result JSON</h4>
              {scenario2?.uncertainty_note && (
                <p className="muted viz-note">{scenario2.uncertainty_note}</p>
              )}
              <div className="chart-explorer-grid">
                {[...autoCharts, ...trajectories].map((chart) => (
                  <div key={chart.id} className="chart-explorer-card">
                    <FlexibleChart
                      title={chart.title}
                      xLabel={chart.x_label}
                      yLabel={chart.y_label}
                      series={chart.series}
                      chartType={(chart.chart_type as ChartType) ?? "line"}
                      large
                    />
                  </div>
                ))}
              </div>
            </div>
          )}

          {data.modelling?.comorbidity_matrix && (
            <div className="viz-panel-section">
              <ComorbidityMatrix matrix={data.modelling.comorbidity_matrix} />
            </div>
          )}
        </>
      )}

      {showCharts && chartBuilder && (
        <ChartExplorer
          workspaceId={workspaceId}
          variables={chartBuilder.variables}
          timeAxis={chartBuilder.time_axis ?? { id: "__time__", label: "Year", category: "Time" }}
          chartTypes={chartTypes}
        />
      )}

      {showCharts && data && (
        <details className="viz-run-details">
          <summary>Run details (metadata, reproducibility)</summary>
          <dl className="repro-dl">
            <dt>Model</dt>
            <dd>{String(data.scenario4.reproducibility.model ?? "—")}</dd>
            <dt>Version</dt>
            <dd>{String(data.scenario4.reproducibility.version ?? "—")}</dd>
            <dt>Seed</dt>
            <dd>{String(data.scenario4.reproducibility.seed ?? "—")}</dd>
            <dt>Intervention</dt>
            <dd>
              {String(
                data.scenario4.reproducibility.intervention ?? data.meta.intervention ?? "—"
              )}
            </dd>
            <dt>Years in output</dt>
            <dd>{data.meta.years?.join(", ") || "—"}</dd>
          </dl>
        </details>
      )}
    </div>
  );
}
