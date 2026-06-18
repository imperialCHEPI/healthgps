import { useMemo, useState } from "react";
import {
  api,
  type ResultChart,
  type ResultChartSeries,
} from "../api/client";
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
  defaultCharts: ResultChart[];
}

interface BuiltChart {
  id: string;
  title: string;
  x_label: string;
  y_label: string;
  chart_type: ChartType;
  series: ResultChartSeries[];
  builtIn?: boolean;
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
        {includeTime && timeAxis && (
          <option value={timeAxis.id}>{timeAxis.label}</option>
        )}
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
  defaultCharts,
}: Props) {
  const [xVar, setXVar] = useState(TIME_AXIS);
  const [yVar, setYVar] = useState(
    variables.find((v) => v.id === "indicators.DALY")?.id ?? variables[0]?.id ?? ""
  );
  const [chartType, setChartType] = useState<ChartType>("line");
  const [baselineOn, setBaselineOn] = useState(true);
  const [interventionOn, setInterventionOn] = useState(true);
  const [customCharts, setCustomCharts] = useState<BuiltChart[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const categories = useMemo(() => {
    const set = new Set(variables.map((v) => v.category));
    return Array.from(set).sort();
  }, [variables]);

  const addChart = async () => {
    if (!yVar) return;
    const sources: string[] = [];
    if (baselineOn) sources.push("Baseline");
    if (interventionOn) sources.push("Intervention");
    if (sources.length === 0) {
      setError("Select at least one scenario (Baseline or Intervention).");
      return;
    }
    if (xVar === yVar) {
      setError("X and Y must be different variables.");
      return;
    }
    setError(null);
    setLoading(true);
    try {
      const res = await api.resultChart(workspaceId, {
        x: xVar,
        y: yVar,
        chartType,
        sources: sources.join(","),
      });
      const built: BuiltChart = {
        id: `${xVar}-${yVar}-${chartType}-${Date.now()}`,
        title: res.title ?? `${res.y_label} vs ${res.x_label}`,
        x_label: res.x_label,
        y_label: res.y_label,
        chart_type: (res.chart_type as ChartType) ?? chartType,
        series: res.series,
      };
      setCustomCharts((prev) => [...prev, built]);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setLoading(false);
    }
  };

  const removeChart = (id: string) => {
    setCustomCharts((prev) => prev.filter((c) => c.id !== id));
  };

  if (variables.length === 0) {
    return (
      <p className="muted">
        No plottable variables in the result JSON yet. Run a simulation and wait
        for HealthGPS_Result_*.json to be written.
      </p>
    );
  }

  const allCharts: BuiltChart[] = [
    ...defaultCharts.map((c) => ({
      id: c.id,
      title: c.title,
      x_label: c.x_label,
      y_label: c.y_label,
      chart_type: ((c as { chart_type?: ChartType }).chart_type ?? "line") as ChartType,
      series: c.series,
      builtIn: true,
    })),
    ...customCharts,
  ];

  return (
    <div className="chart-explorer">
      <div className="chart-builder-panel">
        <h4 className="chart-builder-title">Build a chart</h4>
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
              <input
                type="checkbox"
                checked={baselineOn}
                onChange={(e) => setBaselineOn(e.target.checked)}
              />
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

          <button
            type="button"
            className="primary chart-add-btn"
            disabled={loading}
            onClick={addChart}
          >
            {loading ? "Loading…" : "Add chart"}
          </button>
        </div>
      </div>

      {error && <p className="alert alert-warning">{error}</p>}

      <details className="chart-explorer-vars">
        <summary>All variables in result JSON ({variables.length})</summary>
        <ul className="chart-var-list">
          {variables.map((v) => (
            <li key={v.id}>
              <button
                type="button"
                className="link-button"
                onClick={() => setYVar(v.id)}
              >
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

      <div className="chart-explorer-grid">
        {allCharts.map((chart) => (
          <div key={chart.id} className="chart-explorer-card">
            {!chart.builtIn && (
              <button
                type="button"
                className="chart-remove"
                onClick={() => removeChart(chart.id)}
                title="Remove chart"
              >
                ×
              </button>
            )}
            <span className="chart-type-badge">{chart.chart_type}</span>
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
    </div>
  );
}
