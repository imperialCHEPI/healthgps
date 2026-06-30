import { useCallback, useEffect, useMemo, useState } from "react";
import { api, type ResultChartSeries } from "../api/client";
import FlexibleChart, { type ChartType } from "./FlexibleChart";

const TIME_AXIS = "__time__";

interface Variable {
  id: string;
  label: string;
  category: string;
  unit: string;
}

interface ChartTypeOption {
  id: ChartType;
  label: string;
}

interface Props {
  workspaceId: string;
  variables: Variable[];
  timeAxis: { id: string; label: string; category: string };
  chartTypes: ChartTypeOption[];
}

interface BuiltChart {
  id: string;
  title: string;
  x_label: string;
  y_label: string;
  chart_type: ChartType;
  series: ResultChartSeries[];
}

function VariableSelect({
  id,
  label,
  value,
  onChange,
  variables,
  categories,
  includeTime,
  timeAxis,
}: {
  id: string;
  label: string;
  value: string;
  onChange: (v: string) => void;
  variables: Variable[];
  categories: string[];
  includeTime?: boolean;
  timeAxis?: { id: string; label: string; category: string };
}) {
  return (
    <div className="field chart-axis-field">
      <label htmlFor={id}>{label}</label>
      <select id={id} value={value} onChange={(e) => onChange(e.target.value)}>
        {includeTime && timeAxis && <option value={timeAxis.id}>{timeAxis.label}</option>}
        {categories.map((cat) => (
          <optgroup key={cat} label={cat}>
            {variables
              .filter((v) => v.category === cat)
              .map((v) => (
                <option key={v.id} value={v.id}>
                  {v.label}
                </option>
              ))}
          </optgroup>
        ))}
      </select>
    </div>
  );
}

export default function ChartExplorer({
  workspaceId,
  variables,
  timeAxis,
  chartTypes,
}: Props) {
  const [xVar, setXVar] = useState(TIME_AXIS);
  const [yVar, setYVar] = useState(variables[0]?.id ?? "");
  const [chartType, setChartType] = useState<ChartType>("line");
  const [baselineOn, setBaselineOn] = useState(true);
  const [interventionOn, setInterventionOn] = useState(true);
  const [charts, setCharts] = useState<BuiltChart[]>([]);
  const [preview, setPreview] = useState<BuiltChart | null>(null);
  const [loading, setLoading] = useState(false);
  const [previewLoading, setPreviewLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const categories = useMemo(() => {
    const set = new Set(variables.map((v) => v.category));
    return Array.from(set).sort();
  }, [variables]);

  useEffect(() => {
    if (!yVar && variables[0]?.id) {
      setYVar(variables[0].id);
    }
  }, [variables, yVar]);

  const selectedSources = useCallback(() => {
    const sources: string[] = [];
    if (baselineOn) sources.push("Baseline");
    if (interventionOn) sources.push("Intervention");
    return sources;
  }, [baselineOn, interventionOn]);

  const fetchChart = useCallback(
    async (forPreview: boolean) => {
      if (!yVar) return null;
      const sources = selectedSources();
      if (sources.length === 0) {
        setError("Select at least one scenario (Baseline or Intervention).");
        return null;
      }
      if (xVar === yVar) {
        setError("X and Y must be different variables.");
        return null;
      }
      setError(null);
      if (forPreview) setPreviewLoading(true);
      else setLoading(true);
      try {
        const res = await api.resultChart(workspaceId, {
          x: xVar,
          y: yVar,
          chartType,
          sources: sources.join(","),
        });
        return {
          id: `preview-${xVar}-${yVar}-${chartType}`,
          title: res.title ?? `${res.y_label} vs ${res.x_label}`,
          x_label: res.x_label,
          y_label: res.y_label,
          chart_type: (res.chart_type as ChartType) ?? chartType,
          series: res.series,
        };
      } catch (e) {
        if (forPreview) setPreview(null);
        setError(e instanceof Error ? e.message : String(e));
        return null;
      } finally {
        if (forPreview) setPreviewLoading(false);
        else setLoading(false);
      }
    },
    [workspaceId, xVar, yVar, chartType, selectedSources]
  );

  useEffect(() => {
    if (!yVar || variables.length === 0) {
      setPreview(null);
      return;
    }
    const sources = selectedSources();
    if (sources.length === 0 || xVar === yVar) {
      setPreview(null);
      return;
    }
    const timer = window.setTimeout(() => {
      fetchChart(true).then((built) => {
        if (built) setPreview(built);
      });
    }, 350);
    return () => window.clearTimeout(timer);
  }, [xVar, yVar, chartType, baselineOn, interventionOn, variables.length, fetchChart, selectedSources, yVar]);

  const addChart = async () => {
    const built = await fetchChart(false);
    if (!built) return;
    setCharts((prev) => [
      ...prev,
      { ...built, id: `${built.id}-${Date.now()}` },
    ]);
  };

  if (variables.length === 0) {
    return (
      <p className="muted">
        No plottable variables in the result JSON yet. Run a simulation and wait for
        HealthGPS_Result_*.json to be written.
      </p>
    );
  }

  return (
    <div className="chart-explorer">
      <div className="chart-builder-panel">
        <h4 className="chart-builder-title">Create your own chart</h4>
        <p className="muted chart-builder-hint">
          Pick chart type, X axis, Y axis, and scenarios — the preview updates live as you
          change controls. Click Add chart to keep a view in the grid below.
        </p>
        <div className="chart-explorer-controls">
          <div className="field chart-axis-field">
            <label htmlFor="chart-type">Chart type</label>
            <select
              id="chart-type"
              value={chartType}
              onChange={(e) => setChartType(e.target.value as ChartType)}
            >
              {chartTypes.map((t) => (
                <option key={t.id} value={t.id}>
                  {t.label}
                </option>
              ))}
            </select>
          </div>

          <VariableSelect
            id="chart-x"
            label="X axis"
            value={xVar}
            onChange={setXVar}
            variables={variables}
            categories={categories}
            includeTime
            timeAxis={timeAxis}
          />

          <VariableSelect
            id="chart-y"
            label="Y axis"
            value={yVar}
            onChange={setYVar}
            variables={variables}
            categories={categories}
          />

          <div className="chart-explorer-sources">
            <span className="chart-sources-label">Scenarios</span>
            <label>
              <input type="checkbox" checked={baselineOn} onChange={(e) => setBaselineOn(e.target.checked)} />
              Baseline
            </label>
            <label>
              <input
                type="checkbox"
                checked={interventionOn}
                onChange={(e) => setInterventionOn(e.target.checked)}
              />
              Intervention
            </label>
          </div>

          <button type="button" className="primary chart-add-btn" disabled={loading} onClick={addChart}>
            {loading ? "Loading…" : "Add chart"}
          </button>
        </div>
      </div>

      {error && <p className="alert alert-warning">{error}</p>}

      {(preview || previewLoading) && (
        <div className="viz-panel-section">
          <h4 className="viz-section-title">Live preview</h4>
          {previewLoading && !preview ? (
            <p className="muted">Updating chart…</p>
          ) : preview ? (
            <div className="chart-explorer-card chart-explorer-card--preview">
              <span className="chart-type-badge">{preview.chart_type.replace(/_/g, " ")}</span>
              <FlexibleChart
                title={preview.title}
                xLabel={preview.x_label}
                yLabel={preview.y_label}
                series={preview.series}
                chartType={preview.chart_type}
                large
              />
            </div>
          ) : null}
        </div>
      )}

      <details className="chart-explorer-vars">
        <summary>Browse all variables ({variables.length})</summary>
        <ul className="chart-var-list">
          {variables.map((v) => (
            <li key={v.id}>
              <button type="button" className="link-button" onClick={() => setYVar(v.id)}>
                {v.label}
              </button>
              <span className="muted">
                {v.category}
                {v.unit ? ` · ${v.unit}` : ""}
              </span>
            </li>
          ))}
        </ul>
      </details>

      {charts.length === 0 ? (
        <div className="chart-explorer-empty">
          <p>No saved charts yet. Use Add chart to pin the live preview to this grid.</p>
        </div>
      ) : (
        <div className="chart-explorer-grid">
          {charts.map((chart) => (
            <div key={chart.id} className="chart-explorer-card">
              <button
                type="button"
                className="chart-remove"
                onClick={() => setCharts((prev) => prev.filter((c) => c.id !== chart.id))}
                title="Remove chart"
              >
                ×
              </button>
              <span className="chart-type-badge">{chart.chart_type.replace(/_/g, " ")}</span>
              <FlexibleChart
                title={chart.title}
                xLabel={chart.x_label}
                yLabel={chart.y_label}
                series={chart.series}
                chartType={chart.chart_type}
                large
              />
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
