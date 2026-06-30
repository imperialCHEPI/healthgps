import { useEffect, useState } from "react";
import { api, type ResultChart } from "../api/client";
import LabeledLineChart from "./LabeledLineChart";

interface Props {
  workspaceId: string;
  show: boolean;
}

export default function ResultLineCharts({ workspaceId, show }: Props) {
  const [charts, setCharts] = useState<ResultChart[]>([]);
  const [message, setMessage] = useState<string | null>(null);
  const [intervention, setIntervention] = useState<string | null>(null);
  const [resultFile, setResultFile] = useState<string | null>(null);

  useEffect(() => {
    if (!show) return;
    api
      .resultCharts(workspaceId)
      .then((r) => {
        setCharts(r.charts);
        setMessage(r.message);
        setIntervention(r.experiment?.intervention ?? null);
        setResultFile(r.result_file ?? null);
      })
      .catch(() => setMessage("Could not load result charts"));
  }, [workspaceId, show]);

  if (!show) return null;

  if (charts.length === 0) {
    return (
      <div className="result-charts result-charts--empty">
        <h4 className="sim-chart-title">Simulation results</h4>
        <p className="muted">
          {message ||
            "No aggregate charts yet. Run the simulation to generate HealthGPS_Result_*.json output."}
        </p>
      </div>
    );
  }

  return (
    <div className="result-charts">
      <div className="result-charts-header">
        <h4 className="sim-chart-title">Simulation results</h4>
        <span className="muted result-charts-meta">
          {intervention && <>Policy: {intervention}</>}
          {resultFile && <> · {resultFile}</>}
        </span>
      </div>
      <div className="result-charts-grid">
        {charts.map((chart) => (
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
  );
}
