export interface ChartPoint {
  x: number;
  y: number;
}

export interface ChartSeries {
  name: string;
  color: string;
  points: ChartPoint[];
  yAxis?: "left" | "right";
}

interface Props {
  title?: string;
  xLabel: string;
  yLabel: string;
  y2Label?: string;
  series: ChartSeries[];
  height?: number;
  compact?: boolean;
  showAxisLabels?: boolean;
  formatX?: (x: number) => string;
  formatY?: (y: number) => string;
  formatY2?: (y: number) => string;
}

function niceStep(max: number, ticks: number) {
  const raw = max / ticks;
  const mag = Math.pow(10, Math.floor(Math.log10(raw || 1)));
  const norm = raw / mag;
  const step = norm <= 1 ? mag : norm <= 2 ? 2 * mag : norm <= 5 ? 5 * mag : 10 * mag;
  return step || 1;
}

function axisTicks(min: number, max: number, count = 3) {
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

export default function LabeledLineChart({
  title,
  xLabel,
  yLabel,
  y2Label,
  series,
  height,
  compact = false,
  showAxisLabels,
  formatX = (x) => String(Math.round(x)),
  formatY = (y) => (Math.abs(y) >= 1000 ? `${(y / 1000).toFixed(1)}k` : y.toFixed(1)),
  formatY2 = formatY,
}: Props) {
  const chartHeight = height ?? (compact ? 88 : 140);
  const width = compact ? 280 : 320;
  const margin = compact
    ? { top: 6, right: y2Label ? 28 : 8, bottom: 18, left: 30 }
    : { top: 10, right: y2Label ? 34 : 10, bottom: 24, left: 36 };
  const axisLabels = showAxisLabels ?? !compact;
  const tickCount = compact ? 3 : 4;
  const plotW = width - margin.left - margin.right;
  const plotH = chartHeight - margin.top - margin.bottom;

  const leftSeries = series.filter((s) => (s.yAxis ?? "left") === "left");
  const rightSeries = series.filter((s) => s.yAxis === "right");

  const allX = series.flatMap((s) => s.points.map((p) => p.x));
  const xMin = allX.length ? Math.min(...allX) : 0;
  const xMax = allX.length ? Math.max(...allX) : 1;

  const leftY = leftSeries.flatMap((s) => s.points.map((p) => p.y));
  const rightY = rightSeries.flatMap((s) => s.points.map((p) => p.y));

  const yMin = leftY.length ? Math.min(0, ...leftY) : 0;
  const yMax = leftY.length ? Math.max(...leftY, 1) : 1;
  const y2Min = rightY.length ? Math.min(0, ...rightY) : 0;
  const y2Max = rightY.length ? Math.max(...rightY, 1) : 1;

  const xTicks = axisTicks(xMin, xMax, tickCount);
  const yTicks = axisTicks(yMin, yMax, tickCount);
  const y2Ticks = axisTicks(y2Min, y2Max, tickCount);

  const xScale = (x: number) =>
    margin.left + ((x - xMin) / Math.max(xMax - xMin, 1)) * plotW;
  const yScale = (y: number) =>
    margin.top + plotH - ((y - yMin) / Math.max(yMax - yMin, 1)) * plotH;
  const y2Scale = (y: number) =>
    margin.top + plotH - ((y - y2Min) / Math.max(y2Max - y2Min, 1)) * plotH;

  const lineFor = (pts: ChartPoint[], axis: "left" | "right") =>
    pts
      .map((p) => {
        const y = axis === "right" ? y2Scale(p.y) : yScale(p.y);
        return `${xScale(p.x)},${y}`;
      })
      .join(" ");

  return (
    <div className={`labeled-chart${compact ? " labeled-chart--compact" : ""}`}>
      {(title || series.length > 0) && (
        <div className="labeled-chart-head">
          {title && <h4 className="labeled-chart-title">{title}</h4>}
          {compact && series.length > 0 && (
            <div className="labeled-chart-legend labeled-chart-legend--inline">
              {series.map((s) => (
                <span key={s.name} className="labeled-chart-legend-item">
                  <span className="labeled-chart-swatch" style={{ background: s.color }} />
                  {s.name}
                </span>
              ))}
            </div>
          )}
        </div>
      )}
      <svg
        viewBox={`0 0 ${width} ${chartHeight}`}
        className="labeled-chart-svg"
        role="img"
        aria-label={title ?? "Line chart"}
      >
        {yTicks.map((tick) => (
          <g key={`yg-${tick}`}>
            <line
              x1={margin.left}
              x2={margin.left + plotW}
              y1={yScale(tick)}
              y2={yScale(tick)}
              className="labeled-chart-grid"
            />
            <text x={margin.left - 6} y={yScale(tick) + 3} className="labeled-chart-tick labeled-chart-tick--y">
              {formatY(tick)}
            </text>
          </g>
        ))}

        {y2Label &&
          y2Ticks.map((tick) => (
            <text
              key={`y2g-${tick}`}
              x={margin.left + plotW + 6}
              y={y2Scale(tick) + 3}
              className="labeled-chart-tick labeled-chart-tick--y2"
            >
              {formatY2(tick)}
            </text>
          ))}

        {xTicks.map((tick) => (
          <g key={`xg-${tick}`}>
            <line
              x1={xScale(tick)}
              x2={xScale(tick)}
              y1={margin.top}
              y2={margin.top + plotH}
              className="labeled-chart-grid labeled-chart-grid--light"
            />
            <text
              x={xScale(tick)}
              y={margin.top + plotH + 16}
              className="labeled-chart-tick labeled-chart-tick--x"
            >
              {formatX(tick)}
            </text>
          </g>
        ))}

        <line
          x1={margin.left}
          x2={margin.left}
          y1={margin.top}
          y2={margin.top + plotH}
          className="labeled-chart-axis"
        />
        <line
          x1={margin.left}
          x2={margin.left + plotW}
          y1={margin.top + plotH}
          y2={margin.top + plotH}
          className="labeled-chart-axis"
        />
        {y2Label && (
          <line
            x1={margin.left + plotW}
            x2={margin.left + plotW}
            y1={margin.top}
            y2={margin.top + plotH}
            className="labeled-chart-axis"
          />
        )}

        {series.map((s) => {
          const axis = s.yAxis ?? "left";
          if (s.points.length < 2) return null;
          return (
            <polyline
              key={s.name}
              points={lineFor(s.points, axis)}
              fill="none"
              stroke={s.color}
              strokeWidth={compact ? 1.5 : 2}
              strokeLinejoin="round"
            />
          );
        })}

        {axisLabels && (
          <text
            x={margin.left + plotW / 2}
            y={chartHeight - 2}
            className="labeled-chart-axis-label labeled-chart-axis-label--x"
          >
            {xLabel}
          </text>
        )}
        {axisLabels && (
          <text
            x={10}
            y={margin.top + plotH / 2}
            transform={`rotate(-90 10 ${margin.top + plotH / 2})`}
            className="labeled-chart-axis-label labeled-chart-axis-label--y"
          >
            {yLabel}
          </text>
        )}
        {axisLabels && y2Label && (
          <text
            x={width - 6}
            y={margin.top + plotH / 2}
            transform={`rotate(90 ${width - 6} ${margin.top + plotH / 2})`}
            className="labeled-chart-axis-label labeled-chart-axis-label--y2"
          >
            {y2Label}
          </text>
        )}
      </svg>

      {!compact && series.length > 0 && (
        <div className="labeled-chart-legend">
          {series.map((s) => (
            <span key={s.name} className="labeled-chart-legend-item">
              <span className="labeled-chart-swatch" style={{ background: s.color }} />
              {s.name}
            </span>
          ))}
        </div>
      )}
    </div>
  );
}
