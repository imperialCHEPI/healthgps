import { useMemo, useRef, useState } from "react";
import type { ChartPoint, ChartSeries } from "./LabeledLineChart";

export type ChartType =
  | "line"
  | "area"
  | "bar"
  | "column"
  | "scatter"
  | "step"
  | "smooth"
  | "pie"
  | "stacked_bar"
  | "combo";

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

interface HitPoint {
  series: string;
  color: string;
  x: number;
  y: number;
  px: number;
  py: number;
}

function niceStep(max: number, ticks: number) {
  const raw = max / ticks;
  const mag = Math.pow(10, Math.floor(Math.log10(raw || 1)));
  const norm = raw / mag;
  const step = norm <= 1 ? mag : norm <= 2 ? 2 * mag : norm <= 5 ? 5 * mag : 10 * mag;
  return step || 1;
}

function axisTicks(min: number, max: number, count = 6) {
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

function linePath(pts: ChartPoint[], xScale: (x: number) => number, yScale: (y: number) => number) {
  return pts.map((p) => `${xScale(p.x)},${yScale(p.y)}`).join(" ");
}

function smoothPath(pts: ChartPoint[], xScale: (x: number) => number, yScale: (y: number) => number) {
  if (pts.length < 2) return linePath(pts, xScale, yScale);
  const coords = pts.map((p) => [xScale(p.x), yScale(p.y)] as const);
  let d = `M ${coords[0][0]},${coords[0][1]}`;
  for (let i = 1; i < coords.length; i += 1) {
    const prev = coords[i - 1];
    const cur = coords[i];
    const cx = (prev[0] + cur[0]) / 2;
    d += ` Q ${cx},${prev[1]} ${cur[0]},${cur[1]}`;
  }
  return d;
}

function stepPath(pts: ChartPoint[], xScale: (x: number) => number, yScale: (y: number) => number) {
  if (!pts.length) return "";
  let d = `M ${xScale(pts[0].x)},${yScale(pts[0].y)}`;
  for (let i = 1; i < pts.length; i += 1) {
    d += ` H ${xScale(pts[i].x)} V ${yScale(pts[i].y)}`;
  }
  return d;
}

function areaPath(pts: ChartPoint[], xScale: (x: number) => number, yScale: (y: number) => number, baseY: number) {
  if (!pts.length) return "";
  const top = pts.map((p) => `${xScale(p.x)},${yScale(p.y)}`).join(" L ");
  const firstX = xScale(pts[0].x);
  const lastX = xScale(pts[pts.length - 1].x);
  return `M ${firstX},${baseY} L ${top} L ${lastX},${baseY} Z`;
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
  formatY = (y) => (Math.abs(y) >= 1000 ? `${(y / 1000).toFixed(2)}k` : y.toFixed(2)),
}: Props) {
  const wrapRef = useRef<HTMLDivElement>(null);
  const [hover, setHover] = useState<HitPoint | null>(null);

  const chartHeight = height ?? (large ? 420 : 160);
  const width = large ? 900 : 360;
  const margin = large
    ? { top: 24, right: 28, bottom: 56, left: 68 }
    : { top: 12, right: 12, bottom: 28, left: 40 };
  const plotW = width - margin.left - margin.right;
  const plotH = chartHeight - margin.top - margin.bottom;
  const effectiveType = chartType === "column" ? "bar" : chartType;

  const pieSlices = useMemo(() => {
    if (effectiveType !== "pie") return [];
    return series
      .map((s) => {
        const last = s.points[s.points.length - 1];
        return last ? { name: s.name, value: last.y, color: s.color, x: last.x } : null;
      })
      .filter((s): s is { name: string; value: number; color: string; x: number } => s !== null);
  }, [series, effectiveType]);

  const allX = series.flatMap((s) => s.points.map((p) => p.x));
  const allY = series.flatMap((s) => s.points.map((p) => p.y));

  if (effectiveType === "pie") {
    if (!pieSlices.length) return <p className="muted chart-empty">Not enough data for pie chart.</p>;
    const total = pieSlices.reduce((sum, s) => sum + Math.max(0, s.value), 0) || 1;
    let angle = -Math.PI / 2;
    const cx = width / 2;
    const cy = chartHeight / 2 + 10;
    const r = Math.min(plotW, plotH) / 2.2;
    const paths = pieSlices.map((slice) => {
      const frac = Math.max(0, slice.value) / total;
      const start = angle;
      angle += frac * Math.PI * 2;
      const end = angle;
      const x1 = cx + r * Math.cos(start);
      const y1 = cy + r * Math.sin(start);
      const x2 = cx + r * Math.cos(end);
      const y2 = cy + r * Math.sin(end);
      const largeArc = frac > 0.5 ? 1 : 0;
      const d = `M ${cx} ${cy} L ${x1} ${y1} A ${r} ${r} 0 ${largeArc} 1 ${x2} ${y2} Z`;
      return { ...slice, d, pct: frac * 100 };
    });

    return (
      <div className={`flexible-chart flexible-chart--large flexible-chart--interactive`} ref={wrapRef}>
        <div className="flexible-chart-head">
          {title && <h4 className="flexible-chart-title">{title}</h4>}
        </div>
        <svg viewBox={`0 0 ${width} ${chartHeight}`} className="flexible-chart-svg" role="img">
          {paths.map((slice) => (
            <path
              key={slice.name}
              d={slice.d}
              fill={slice.color}
              opacity={hover?.series === slice.name ? 1 : 0.88}
              onMouseEnter={() =>
                setHover({
                  series: slice.name,
                  color: slice.color,
                  x: slice.x,
                  y: slice.value,
                  px: 0,
                  py: 0,
                })
              }
              onMouseLeave={() => setHover(null)}
            />
          ))}
          <text x={cx} y={chartHeight - 12} className="flexible-chart-axis-label flexible-chart-axis-label--x" textAnchor="middle">
            {yLabel} at {formatX(paths[0]?.x ?? 0)}
          </text>
        </svg>
        <div className="flexible-chart-legend flexible-chart-legend--below">
          {paths.map((s) => (
            <span key={s.name} className="flexible-chart-legend-item">
              <span className="flexible-chart-swatch" style={{ background: s.color }} />
              {s.name}: {formatY(s.value)} ({s.pct.toFixed(1)}%)
            </span>
          ))}
        </div>
        {hover && (
          <div className="chart-tooltip chart-tooltip--static">
            <strong>{hover.series}</strong>
            <div>{yLabel}: {formatY(hover.y)}</div>
            <div>{xLabel}: {formatX(hover.x)}</div>
          </div>
        )}
      </div>
    );
  }

  if (!allX.length || !allY.length) {
    return <p className="muted chart-empty">Not enough data to plot.</p>;
  }

  const xMin = Math.min(...allX);
  const xMax = Math.max(...allX);
  const yMin = Math.min(0, ...allY);
  const yMax = Math.max(...allY, 1);
  const xTicks = axisTicks(xMin, xMax);
  const yTicks = axisTicks(yMin, yMax);
  const xScale = (x: number) => margin.left + ((x - xMin) / Math.max(xMax - xMin, 1)) * plotW;
  const yScale = (y: number) => margin.top + plotH - ((y - yMin) / Math.max(yMax - yMin, 1)) * plotH;
  const baseY = yScale(yMin);

  const categories = Array.from(new Set(allX)).sort((a, b) => a - b);
  const catIndex = new Map(categories.map((c, i) => [c, i]));
  const maps = effectiveType === "bar" || effectiveType === "stacked_bar" || effectiveType === "combo"
    ? pointMap(series)
    : null;
  const groupW = plotW / Math.max(categories.length, 1);
  const barW =
    effectiveType === "stacked_bar"
      ? groupW * 0.55
      : (groupW * 0.72) / Math.max(series.length, 1);

  const hitPoints: HitPoint[] = series.flatMap((s) =>
    s.points.map((p) => ({
      series: s.name,
      color: s.color,
      x: p.x,
      y: p.y,
      px: xScale(p.x),
      py: yScale(p.y),
    }))
  );

  const onSvgMove = (e: React.MouseEvent<SVGSVGElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const scaleX = width / rect.width;
    const scaleY = chartHeight / rect.height;
    const mx = (e.clientX - rect.left) * scaleX;
    const my = (e.clientY - rect.top) * scaleY;
    let best: HitPoint | null = null;
    let bestDist = 18;
    for (const pt of hitPoints) {
      const d = Math.hypot(pt.px - mx, pt.py - my);
      if (d < bestDist) {
        bestDist = d;
        best = pt;
      }
    }
    setHover(best);
  };

  const renderLineLike = (type: "line" | "smooth" | "step" | "area" | "combo-line") =>
    series.map((s, idx) => {
      if (s.points.length < 2 && type !== "area") return null;
      const isComboLine = type === "combo-line";
      const strokeW = isComboLine ? 3 : large ? 2.5 : 2;
      if (type === "area") {
        return (
          <path
            key={s.name}
            d={areaPath(s.points, xScale, yScale, baseY)}
            fill={s.color}
            opacity={0.28}
            stroke={s.color}
            strokeWidth={1.5}
          />
        );
      }
      const d =
        type === "step"
          ? stepPath(s.points, xScale, yScale)
          : type === "smooth" || type === "combo-line"
            ? smoothPath(s.points, xScale, yScale)
            : linePath(s.points, xScale, yScale);
      if (isComboLine && idx === 0) return null;
      if (type === "step" || type === "smooth" || type === "combo-line") {
        return (
          <path
            key={s.name}
            d={d}
            fill="none"
            stroke={s.color}
            strokeWidth={strokeW}
            strokeLinejoin="round"
            opacity={hover && hover.series !== s.name ? 0.45 : 1}
          />
        );
      }
      return (
        <polyline
          key={s.name}
          points={d}
          fill="none"
          stroke={s.color}
          strokeWidth={strokeW}
          strokeLinejoin="round"
          opacity={hover && hover.series !== s.name ? 0.45 : 1}
        />
      );
    });

  return (
    <div
      className={`flexible-chart${large ? " flexible-chart--large" : ""} flexible-chart--interactive`}
      ref={wrapRef}
    >
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
        onMouseMove={onSvgMove}
        onMouseLeave={() => setHover(null)}
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
            <text x={margin.left - 10} y={yScale(tick) + 4} className="flexible-chart-tick flexible-chart-tick--y">
              {formatY(tick)}
            </text>
          </g>
        ))}

        {effectiveType !== "bar" &&
          effectiveType !== "stacked_bar" &&
          xTicks.map((tick) => (
            <g key={`xg-${tick}`}>
              <line
                x1={xScale(tick)}
                x2={xScale(tick)}
                y1={margin.top}
                y2={margin.top + plotH}
                className="flexible-chart-grid flexible-chart-grid--light"
              />
              <text x={xScale(tick)} y={margin.top + plotH + 22} className="flexible-chart-tick flexible-chart-tick--x" textAnchor="middle">
                {formatX(tick)}
              </text>
            </g>
          ))}

        {categories.map((cat) => {
          const i = catIndex.get(cat) ?? 0;
          const cx = margin.left + groupW * i + groupW / 2;
          return (
            <text
              key={`cat-${cat}`}
              x={cx}
              y={margin.top + plotH + 22}
              className="flexible-chart-tick flexible-chart-tick--x"
              textAnchor="middle"
              opacity={effectiveType === "bar" || effectiveType === "stacked_bar" || effectiveType === "combo" ? 1 : 0}
            >
              {formatX(cat)}
            </text>
          );
        })}

        <line x1={margin.left} x2={margin.left} y1={margin.top} y2={margin.top + plotH} className="flexible-chart-axis" />
        <line x1={margin.left} x2={margin.left + plotW} y1={margin.top + plotH} y2={margin.top + plotH} className="flexible-chart-axis" />

        {effectiveType === "line" && renderLineLike("line")}
        {effectiveType === "smooth" && renderLineLike("smooth")}
        {effectiveType === "step" && renderLineLike("step")}
        {effectiveType === "area" && renderLineLike("area")}

        {(effectiveType === "scatter" || effectiveType === "line" || effectiveType === "smooth" || effectiveType === "step" || effectiveType === "area" || effectiveType === "combo") &&
          series.flatMap((s) =>
            s.points.map((p, i) => (
              <circle
                key={`${s.name}-${i}`}
                cx={xScale(p.x)}
                cy={yScale(p.y)}
                r={hover?.series === s.name && hover?.x === p.x && hover?.y === p.y ? 7 : large ? 5 : 4}
                fill={s.color}
                opacity={hover && hover.series !== s.name ? 0.35 : 0.9}
                className="chart-hit-point"
              />
            ))
          )}

        {effectiveType === "combo" && renderLineLike("combo-line")}

        {effectiveType === "bar" &&
          maps &&
          series.map((s, si) => {
            const byX = maps.get(s.name);
            if (!byX) return null;
            return categories.map((cat) => {
              const y = byX.get(cat);
              if (y === undefined) return null;
              const i = catIndex.get(cat) ?? 0;
              const groupLeft = margin.left + groupW * i + groupW * 0.14;
              const x = groupLeft + barW * si;
              const yTop = yScale(y);
              return (
                <rect
                  key={`${s.name}-${cat}`}
                  x={x}
                  y={yTop}
                  width={Math.max(barW - 2, 4)}
                  height={Math.max(margin.top + plotH - yTop, 0)}
                  fill={s.color}
                  rx={2}
                  opacity={hover && hover.series !== s.name ? 0.45 : 0.92}
                />
              );
            });
          })}

        {effectiveType === "stacked_bar" &&
          maps &&
          categories.map((cat) => {
            const i = catIndex.get(cat) ?? 0;
            const x = margin.left + groupW * i + groupW * 0.22;
            let stackY = margin.top + plotH;
            return series.map((s) => {
              const y = maps.get(s.name)?.get(cat);
              if (y === undefined) return null;
              const h = ((y - yMin) / Math.max(yMax - yMin, 1)) * plotH;
              stackY -= h;
              return (
                <rect
                  key={`${s.name}-${cat}-stack`}
                  x={x}
                  y={stackY}
                  width={barW}
                  height={Math.max(h, 0)}
                  fill={s.color}
                  opacity={hover && hover.series !== s.name ? 0.45 : 0.92}
                />
              );
            });
          })}

        {effectiveType === "combo" &&
          maps &&
          series.slice(0, 1).map((s) =>
            categories.map((cat) => {
              const y = maps.get(s.name)?.get(cat);
              if (y === undefined) return null;
              const i = catIndex.get(cat) ?? 0;
              const x = margin.left + groupW * i + groupW * 0.2;
              const yTop = yScale(y);
              return (
                <rect
                  key={`combo-bar-${s.name}-${cat}`}
                  x={x}
                  y={yTop}
                  width={barW * 1.6}
                  height={Math.max(margin.top + plotH - yTop, 0)}
                  fill={s.color}
                  opacity={0.35}
                  rx={2}
                />
              );
            })
          )}

        {hover && (
          <g>
            <line x1={hover.px} x2={hover.px} y1={margin.top} y2={margin.top + plotH} className="chart-crosshair" />
            <line x1={margin.left} x2={margin.left + plotW} y1={hover.py} y2={hover.py} className="chart-crosshair" />
          </g>
        )}

        <text x={margin.left + plotW / 2} y={chartHeight - 8} className="flexible-chart-axis-label flexible-chart-axis-label--x" textAnchor="middle">
          {xLabel}
        </text>
        <text x={18} y={margin.top + plotH / 2} transform={`rotate(-90 18 ${margin.top + plotH / 2})`} className="flexible-chart-axis-label flexible-chart-axis-label--y" textAnchor="middle">
          {yLabel}
        </text>
      </svg>

      {hover && (
        <div className="chart-tooltip">
          <strong style={{ color: hover.color }}>{hover.series}</strong>
          <div>
            {xLabel}: <strong>{formatX(hover.x)}</strong>
          </div>
          <div>
            {yLabel}: <strong>{formatY(hover.y)}</strong>
          </div>
        </div>
      )}
    </div>
  );
}
