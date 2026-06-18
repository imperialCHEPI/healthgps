import { type RunTelemetry, type ScenarioTimeline } from "../api/client";

interface Props {
  telemetry: RunTelemetry;
  active: boolean;
}

function SingleTimeline({
  label,
  timeline,
  startYear,
  stopYear,
  variant,
  pulse,
}: {
  label: string;
  timeline: ScenarioTimeline;
  startYear: number;
  stopYear: number;
  variant: "baseline" | "intervention";
  pulse: boolean;
}) {
  const years: number[] = [];
  for (let y = startYear; y <= stopYear; y += 1) {
    years.push(y);
  }
  const span = Math.max(1, stopYear - startYear);
  const current = timeline.current_year ?? startYear;
  const playheadPct = Math.min(
    100,
    Math.max(0, ((current - startYear) / span) * 100)
  );

  return (
    <div
      className={`sim-timeline sim-timeline--${variant}${
        pulse && timeline.active ? " sim-timeline--pulse" : ""
      }${!timeline.active ? " sim-timeline--idle" : ""}`}
    >
      <div className="sim-timeline-head">
        <span className="sim-timeline-label">{label}</span>
        <span className="sim-timeline-year">{timeline.active ? current : "—"}</span>
      </div>

      <div className="sim-timeline-track-wrap">
        <div className="sim-timeline-track">
          <div
            className="sim-timeline-fill"
            style={{ width: `${timeline.progress_pct}%` }}
          />
          <div
            className="sim-timeline-playhead"
            style={{ left: `${playheadPct}%` }}
            aria-hidden
          />
        </div>
        <div className="sim-timeline-ticks">
          {years.map((y) => {
            const pct = ((y - startYear) / span) * 100;
            const isCurrent = timeline.active && timeline.current_year === y;
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
          {startYear} → {stopYear}
        </span>
        <span>{timeline.progress_pct.toFixed(0)}% complete</span>
      </div>
    </div>
  );
}

export default function SimulationTimeline({ telemetry, active }: Props) {
  const t = telemetry;
  const baseline: ScenarioTimeline = t.baseline_timeline ?? {
    current_year: t.current_year ?? t.start_year,
    progress_pct: t.year_progress_pct,
    active: t.phase !== "idle",
  };
  const intervention: ScenarioTimeline = t.intervention_timeline ?? {
    current_year: t.start_year,
    progress_pct: 0,
    active: false,
  };
  const pulse =
    active && (t.phase === "simulating" || t.phase === "policy");

  return (
    <div className="sim-timeline-dual">
      <SingleTimeline
        label="Baseline clock"
        timeline={baseline}
        startYear={t.start_year}
        stopYear={t.stop_year}
        variant="baseline"
        pulse={pulse && baseline.active}
      />
      <SingleTimeline
        label="Intervention clock"
        timeline={intervention}
        startYear={t.start_year}
        stopYear={t.stop_year}
        variant="intervention"
        pulse={pulse && intervention.active}
      />
    </div>
  );
}
