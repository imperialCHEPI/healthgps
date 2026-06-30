import type { BurdenBar } from "../../api/client";

interface Props {
  bars: BurdenBar[];
}

export default function BurdenDeltaChart({ bars }: Props) {
  if (bars.length === 0) return null;

  const maxVal = Math.max(...bars.flatMap((b) => [b.baseline, b.intervention, Math.abs(b.delta)]), 1);

  return (
    <div className="burden-chart">
      <h4 className="viz-section-title">Burden difference (baseline vs intervention)</h4>
      <div className="burden-chart-rows">
        {bars.map((b) => (
          <div key={b.id} className="burden-chart-row">
            <span className="burden-chart-label">{b.label}</span>
            <div className="burden-chart-bars">
              <div className="burden-bar-wrap">
                <div
                  className="burden-bar burden-bar--baseline"
                  style={{ width: `${(b.baseline / maxVal) * 100}%` }}
                  title={`Baseline ${b.baseline.toLocaleString()}`}
                />
                <span className="burden-bar-val">{b.baseline.toLocaleString(undefined, { maximumFractionDigits: 0 })}</span>
              </div>
              <div className="burden-bar-wrap">
                <div
                  className="burden-bar burden-bar--intervention"
                  style={{ width: `${(b.intervention / maxVal) * 100}%` }}
                  title={`Intervention ${b.intervention.toLocaleString()}`}
                />
                <span className="burden-bar-val">{b.intervention.toLocaleString(undefined, { maximumFractionDigits: 0 })}</span>
              </div>
            </div>
            <span className={`burden-delta${b.delta < 0 ? " burden-delta--good" : b.delta > 0 ? " burden-delta--bad" : ""}`}>
              Δ {b.delta > 0 ? "+" : ""}
              {b.delta.toLocaleString(undefined, { maximumFractionDigits: 0 })}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}
