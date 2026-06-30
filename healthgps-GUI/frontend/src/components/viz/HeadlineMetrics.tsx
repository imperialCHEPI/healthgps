import type { HeadlineMetric } from "../../api/client";

interface Props {
  items: HeadlineMetric[];
}

export default function HeadlineMetrics({ items }: Props) {
  if (items.length === 0) return null;

  return (
    <div className="headline-metrics">
      {items.map((h) => (
        <div key={h.id} className="headline-metric-card">
          <span className="headline-metric-value">
            {h.delta > 0 ? "+" : ""}
            {h.unit === "pp" ? h.delta.toFixed(2) : h.delta.toLocaleString(undefined, { maximumFractionDigits: 1 })}
            {h.unit === "pp" ? " pp" : ""}
          </span>
          <p className="headline-metric-text">{h.headline}</p>
        </div>
      ))}
    </div>
  );
}
