import { useEffect, useState } from "react";
import { api, type VisualizationBundle } from "../api/client";
import ChartExplorer from "./ChartExplorer";
import FlexibleChart, { type ChartType } from "./FlexibleChart";
import PipelineGraph from "./viz/PipelineGraph";

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

  const chartTypes = (chartBuilder?.chart_types ?? [
    { id: "line", label: "Line" },
    { id: "area", label: "Area" },
    { id: "bar", label: "Bar" },
    { id: "scatter", label: "Scatter" },
  ]) as { id: ChartType; label: string }[];

  return (
    <div className="viz-hub">
      <div className="viz-hub-header">
        <h3 className="grid-card-title">Results &amp; charts</h3>
        {(data?.meta?.result_file || chartBuilder?.result_file) && (
          <span className="muted viz-hub-meta">
            {data?.meta?.result_file ?? chartBuilder?.result_file}
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
            <dd>{String(data.scenario4.reproducibility.intervention ?? data.meta.intervention ?? "—")}</dd>
            <dt>Years in output</dt>
            <dd>{data.meta.years?.join(", ") || "—"}</dd>
          </dl>
          {data.scenario2.headlines.length > 0 && (
            <div className="viz-headline-strip">
              {data.scenario2.headlines.map((h) => (
                <div key={h.id} className="viz-headline-chip">
                  <span className="muted">{h.label}</span>
                  <strong>
                    {h.delta > 0 ? "+" : ""}
                    {typeof h.delta_pct === "number" ? `${h.delta_pct.toFixed(1)}%` : h.delta.toFixed(2)}
                  </strong>
                </div>
              ))}
            </div>
          )}
          {data.scenario2.trajectories.length > 0 && (
            <div className="viz-suggested-section">
              <p className="muted">
                Quick views from the result file — add your own charts above for full control.
              </p>
              <div className="chart-explorer-grid chart-explorer-grid--suggested">
                {data.scenario2.trajectories.map((chart) => (
                  <div key={chart.id} className="chart-explorer-card chart-explorer-card--suggested">
                    <FlexibleChart
                      title={chart.title}
                      xLabel={chart.x_label}
                      yLabel={chart.y_label}
                      series={chart.series}
                      chartType="line"
                      large
                    />
                  </div>
                ))}
              </div>
            </div>
          )}
        </details>
      )}
    </div>
  );
}
