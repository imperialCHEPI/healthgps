import { type RunTelemetry } from "../api/client";

interface Props {
  telemetry: RunTelemetry;
  active: boolean;
}

export default function SimulationTimeline({ telemetry, active }: Props) {
  const t = telemetry;
  const years: number[] = [];
  for (let y = t.start_year; y <= t.stop_year; y += 1) {
    years.push(y);
  }
  const span = Math.max(1, t.stop_year - t.start_year);
  const current = t.current_year ?? t.start_year;
  const playheadPct = Math.min(
    100,
    Math.max(0, ((current - t.start_year) / span) * 100)
  );

  return (
    <div
      className={`sim-timeline${active && t.phase === "simulating" ? " sim-timeline--pulse" : ""}${
        active && t.phase === "policy" ? " sim-timeline--policy" : ""
      }`}
    >
      <div className="sim-timeline-head">
        <span className="sim-timeline-label">Simulation clock</span>
        <span className="sim-timeline-year">
          {t.current_year != null ? t.current_year : "—"}
        </span>
      </div>

      <div className="sim-timeline-track-wrap">
        <div className="sim-timeline-track">
          <div
            className="sim-timeline-fill"
            style={{ width: `${t.year_progress_pct}%` }}
          />
          <div
            className="sim-timeline-playhead"
            style={{ left: `${playheadPct}%` }}
            aria-hidden
          />
        </div>
        <div className="sim-timeline-ticks">
          {years.map((y) => {
            const pct = ((y - t.start_year) / span) * 100;
            const isCurrent = t.current_year === y;
            return (
              <span
                key={y}
                className={`sim-timeline-tick${isCurrent ? " sim-timeline-tick--current" : ""}`}
                style={{ left: `${pct}%` }}
              >
                {y}
              </span>
            );
          })}
        </div>
      </div>

      <div className="sim-timeline-foot">
        <span>
          {t.start_year} → {t.stop_year}
        </span>
        <span>{t.year_progress_pct.toFixed(0)}% complete</span>
      </div>
    </div>
  );
}
