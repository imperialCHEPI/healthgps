import type { StratumDumbbell } from "../../api/client";

interface Props {
  items: StratumDumbbell[];
  outcome?: string;
}

export default function DumbbellChart({ items, outcome }: Props) {
  if (items.length === 0) return null;

  const vals = items.flatMap((d) => [d.baseline, d.intervention]);
  const min = Math.min(...vals);
  const max = Math.max(...vals);
  const span = Math.max(max - min, 0.01);

  const x = (v: number) => ((v - min) / span) * 100;

  return (
    <div className="dumbbell-chart">
      <h4 className="viz-section-title">
        Equity dumbbells{outcome ? ` — ${outcome}` : ""}
      </h4>
      {items.map((d) => (
        <div key={d.stratum} className="dumbbell-row">
          <span className="dumbbell-label">{d.stratum}</span>
          <div className="dumbbell-track">
            <span className="dumbbell-dot dumbbell-dot--base" style={{ left: `${x(d.baseline)}%` }} title={`Baseline ${d.baseline.toFixed(2)}`} />
            <span className="dumbbell-line" style={{ left: `${x(Math.min(d.baseline, d.intervention))}%`, width: `${Math.abs(x(d.intervention) - x(d.baseline))}%` }} />
            <span className="dumbbell-dot dumbbell-dot--inter" style={{ left: `${x(d.intervention)}%` }} title={`Intervention ${d.intervention.toFixed(2)}`} />
          </div>
          <span className="dumbbell-delta">
            {d.delta > 0 ? "+" : ""}
            {d.delta.toFixed(2)}
          </span>
        </div>
      ))}
      <div className="dumbbell-legend">
        <span><i className="dumbbell-swatch dumbbell-swatch--base" /> Baseline</span>
        <span><i className="dumbbell-swatch dumbbell-swatch--inter" /> Intervention</span>
      </div>
    </div>
  );
}
