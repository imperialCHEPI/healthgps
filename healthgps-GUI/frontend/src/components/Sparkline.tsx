interface Props {
  data: number[];
  label: string;
  unit: string;
  color?: string;
  current?: number;
}

export default function Sparkline({
  data,
  label,
  unit,
  color = "#b91c3c",
  current,
}: Props) {
  const w = 200;
  const h = 48;
  const raw = data.length > 0 ? data : current != null && current > 0 ? [current, current] : [0];
  const values = raw.length === 1 ? [raw[0], raw[0]] : raw;
  const max = Math.max(...values, 1);
  const pts = values
    .map((v, i) => {
      const x = (i / Math.max(values.length - 1, 1)) * w;
      const y = h - (v / max) * (h - 4) - 2;
      return `${x},${y}`;
    })
    .join(" ");

  return (
    <div className="sparkline">
      <div className="sparkline-header">
        <span className="sparkline-label">{label}</span>
        <span className="sparkline-value">
          {current != null ? current.toFixed(1) : "—"}
          {unit}
        </span>
      </div>
      <svg viewBox={`0 0 ${w} ${h}`} className="sparkline-svg" aria-hidden>
        <polyline points={pts} fill="none" stroke={color} strokeWidth="2" />
      </svg>
    </div>
  );
}
