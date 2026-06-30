import LabeledLineChart, { type ChartPoint } from "./LabeledLineChart";

interface Props {
  cpuHistory: number[];
  memoryHistory: number[];
  cpuCurrent: number;
  memoryCurrent: number;
}

function toPoints(values: number[]): ChartPoint[] {
  return values.map((y, i) => ({ x: i, y }));
}

export default function ResourceMonitorChart({
  cpuHistory,
  memoryHistory,
  cpuCurrent,
  memoryCurrent,
}: Props) {
  const cpu =
    cpuHistory.length > 0
      ? cpuHistory
      : cpuCurrent > 0
        ? [cpuCurrent, cpuCurrent]
        : [];
  const mem =
    memoryHistory.length > 0
      ? memoryHistory
      : memoryCurrent > 0
        ? [memoryCurrent, memoryCurrent]
        : [];

  if (cpu.length === 0 && mem.length === 0) {
    return null;
  }

  const series = [];
  if (cpu.length >= 2) {
    series.push({
      name: `CPU ${cpuCurrent.toFixed(0)}%`,
      color: "#b91c3c",
      points: toPoints(cpu),
      yAxis: "left" as const,
    });
  }
  if (mem.length >= 2) {
    series.push({
      name: `Mem ${memoryCurrent.toFixed(0)} MB`,
      color: "#334155",
      points: toPoints(mem),
      yAxis: "right" as const,
    });
  }

  if (series.length === 0) return null;

  return (
    <div className="resource-monitor-chart">
      <LabeledLineChart
        title="Resources"
        xLabel="s"
        yLabel="CPU %"
        y2Label={mem.length >= 2 ? "MB" : undefined}
        series={series}
        compact
        formatX={(x) => String(x)}
        formatY={(y) => `${y.toFixed(0)}`}
        formatY2={(y) => `${y.toFixed(0)}`}
      />
    </div>
  );
}
