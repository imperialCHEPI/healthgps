import type { ChartPoint, ChartSeries } from "./LabeledLineChart";

export type ChartType = "line" | "bar" | "scatter";

interface Props {
  title?: string;
  xLabel: string;
  yLabel: string;
  series: ChartSeries[];
  chartType?: ChartType;
  height?: number;
  large?: boolean;
  formatX?: (x: number) => string;
  formatY?: (y: number) => string;
}

function niceStep(max: number, ticks: number) {
  const raw = max / ticks;
  const mag = Math.pow(10, Math.floor(Math.log10(raw || 1)));
  const norm = raw / mag;
  const step = norm <= 1 ? mag : norm <= 2 ? 2 * mag : norm <= 5 ? 5 * mag : 10 * mag;
  return step || 1;
}

function axisTicks(min: number, max: number, count = 5) {
  if (min === max) {
    const pad = Math.abs(min) * 0.1 || 1;
    min -= pad;
    max += pad;
  }
  const step = niceStep(max - min, count);
  const start = Math.floor(min / step) * step;
  const values: number[] = [];
  for (let v = start; v <= max + step * 0.01; v += step) {
    if (v >= min - step * 0.01) values.push(v);
    if (values.length > count + 1) break;
  }
  return values.length ? values : [min, max];
}

function pointMap(series: ChartSeries[]) {
  const map = new Map<string, Map<number, number>>();
  for (const s of series) {
    const byX = new Map<number, number>();
    for (const p of s.points) byX.set(p.x, p.y);
    map.set(s.name, byX);
  }
  return map;
}

export default function FlexibleChart({
  title,
  xLabel,
  yLabel,
  series,
  chartType = "line",
  height,
  large = false,
  formatX = (x) => (Number.isInteger(x) ? String(x) : x.toFixed(1)),
  formatY = (y) => (Math.abs(y) >= 1000 ? `${(y / 1000).toFixed(1)}k` : y.toFixed(1)),
}: Props) {
  const chartHeight = height ?? (large ? 300 : 140);
  const width = large ? 640 : 320;
  const margin = large
    ? { top: 16, right: 16, bottom: 44, left: 52 }
    : { top: 10, right: 10, bottom: 24, left: 36 };
  const tickCount = large ? 5 : 4;
  const plotW = width - margin.left - margin.right;
  const plotH = chartHeight - margin.top - margin.bottom;

  const allX = series.flatMap((s) => s.points.map((p) => p.x));
  const allY = series.flatMap((s) => s.points.map((p) => p.y));
  if (!allX.length || !allY.length) {
    return <p className="muted chart-empty">Not enough data to plot.</p>;
  }

  const xMin = Math.min(...allX);
  const xMax = Math.max(...allX);
  const yMin = Math.min(0, ...allY);
  const yMax = Math.max(...allY, 1);

  const xTicks = axisTicks(xMin, xMax, tickCount);
  const yTicks = axisTicks(yMin, yMax, tickCount);

  const xScale = (x: number) =>
    margin.left + ((x - xMin) / Math.max(xMax - xMin, 1)) * plotW;
  const yScale = (y: number) =>
    margin.top + plotH - ((y - yMin) / Math.max(yMax - yMin, 1)) * plotH;

  const categories =
    chartType === "bar"
      ? Array.from(new Set(allX)).sort((a, b) => a - b)
      : [];
  const catIndex = new Map(categories.map((c, i) => [c, i]));
  const maps = chartType === "bar" ? pointMap(series) : null;
  const groupW = chartType === "bar" ? plotW / Math.max(categories.length, 1) : 0;
  const barW = chartType === "bar" ? (groupW * 0.7) / Math.max(series.length, 1) : 0;

  const lineFor = (pts: ChartPoint[]) =>
    pts
      .map((p) => `${xScale(p.x)},${yScale(p.y)}`)
      .join(" ");

  return (
    <div className={`flexible-chart${large ? " flexible-chart--large" : ""}`}>
      <div className="flexible-chart-head">
        {title && <h4 className="flexible-chart-title">{title}</h4>}
        <div className="flexible-chart-legend">
          {series.map((s) => (
            <span key={s.name} className="flexible-chart-legend-item">
              <span className="flexible-chart-swatch" style={{ background: s.color }} />
              {s.name}
            </span>
          ))}
        </div>
      </div>

      <svg
        viewBox={`0 0 ${width} ${chartHeight}`}
        className="flexible-chart-svg"
        role="img"
        aria-label={title ?? "Chart"}
        preserveAspectRatio="xMidYMid meet"
      >
        {yTicks.map((tick) => (
          <g key={`yg-${tick}`}>
            <line
              x1={margin.left}
              x2={margin.left + plotW}
              y1={yScale(tick)}
              y2={yScale(tick)}
              className="flexible-chart-grid"
            />
            <text
              x={margin.left - 8}
              y={yScale(tick) + 4}
              className="flexible-chart-tick flexible-chart-tick--y"
            >
              {formatY(tick)}
            </text>
          </g>
        ))}

        {chartType !== "bar" &&
          xTicks.map((tick) => (
            <g key={`xg-${tick}`}>
              <line
                x1={xScale(tick)}
                x2={xScale(tick)}
                y1={margin.top}
                y2={margin.top + plotH}
                className="flexible-chart-grid flexible-chart-grid--light"
              />
              <text
                x={xScale(tick)}
                y={margin.top + plotH + 18}
                className="flexible-chart-tick flexible-chart-tick--x"
              >
                {formatX(tick)}
              </text>
            </g>
          ))}

        {chartType === "bar" &&
          categories.map((cat) => {
            const i = catIndex.get(cat) ?? 0;
            const cx = margin.left + groupW * i + groupW / 2;
            return (
              <text
                key={`cat-${cat}`}
                x={cx}
                y={margin.top + plotH + 18}
                className="flexible-chart-tick flexible-chart-tick--x"
                textAnchor="middle"
              >
                {formatX(cat)}
              </text>
            );
          })}

        <line
          x1={margin.left}
          x2={margin.left}
          y1={margin.top}
          y2={margin.top + plotH}
          className="flexible-chart-axis"
        />
        <line
          x1={margin.left}
          x2={margin.left + plotW}
          y1={margin.top + plotH}
          y2={margin.top + plotH}
          className="flexible-chart-axis"
        />

        {chartType === "line" &&
          series.map((s) =>
            s.points.length >= 2 ? (
              <polyline
                key={s.name}
                points={lineFor(s.points)}
                fill="none"
                stroke={s.color}
                strokeWidth={large ? 2.5 : 2}
                strokeLinejoin="round"
              />
            ) : null
          )}

        {chartType === "scatter" &&
          series.flatMap((s) =>
            s.points.map((p, i) => (
              <circle
                key={`${s.name}-${i}`}
                cx={xScale(p.x)}
                cy={yScale(p.y)}
                r={large ? 5 : 4}
                fill={s.color}
                opacity={0.85}
              />
            ))
          )}

        {chartType === "bar" &&
          maps &&
          series.map((s, si) => {
            const byX = maps.get(s.name);
            if (!byX) return null;
            return categories.map((cat) => {
              const y = byX.get(cat);
              if (y === undefined) return null;
              const i = catIndex.get(cat) ?? 0;
              const groupLeft = margin.left + groupW * i + groupW * 0.15;
              const x = groupLeft + barW * si;
              const yTop = yScale(y);
              const barH = margin.top + plotH - yTop;
              return (
                <rect
                  key={`${s.name}-${cat}`}
                  x={x}
                  y={yTop}
                  width={Math.max(barW - 2, 4)}
                  height={Math.max(barH, 0)}
                  fill={s.color}
                  rx={2}
                />
              );
            });
          })}

        <text
          x={margin.left + plotW / 2}
          y={chartHeight - 6}
          className="flexible-chart-axis-label flexible-chart-axis-label--x"
          textAnchor="middle"
        >
          {xLabel}
        </text>
        <text
          x={14}
          y={margin.top + plotH / 2}
          transform={`rotate(-90 14 ${margin.top + plotH / 2})`}
          className="flexible-chart-axis-label flexible-chart-axis-label--y"
          textAnchor="middle"
        >
          {yLabel}
        </text>
      </svg>
    </div>
  );
}
