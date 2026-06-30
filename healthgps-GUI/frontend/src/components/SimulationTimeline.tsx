import { type RunTelemetry, type ScenarioTimeline } from "../api/client";

interface Props {
  telemetry: RunTelemetry;
  active: boolean;
}

function statusLabel(timeline: ScenarioTimeline & { status?: string }) {
  const status = timeline.status ?? (timeline.active ? "running" : "waiting");
  if (status === "complete") return "Complete";
  if (status === "skipped") return "No policy run";
  if (status === "waiting") return "Waiting";
  return "Running";
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
  timeline: ScenarioTimeline & { status?: string };
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
  const playheadPct = Math.min(100, Math.max(0, ((current - startYear) / span) * 100));
  const done = timeline.status === "complete" || timeline.progress_pct >= 100;
  const waiting = timeline.status === "waiting" || timeline.status === "skipped";

  return (
    <div
      className={`sim-timeline sim-timeline--${variant}${
        pulse && timeline.active ? " sim-timeline--pulse" : ""
      }${waiting ? " sim-timeline--idle" : ""}${done ? " sim-timeline--done" : ""}`}
    >
      <div className="sim-timeline-head">
        <span className="sim-timeline-label">{label}</span>
        <span className="sim-timeline-year">{waiting ? "—" : current}</span>
      </div>

      <div className="sim-timeline-track-wrap">
        <div className="sim-timeline-track">
          <div className="sim-timeline-fill" style={{ width: `${timeline.progress_pct}%` }} />
          <div className="sim-timeline-playhead" style={{ left: `${playheadPct}%` }} aria-hidden />
        </div>
        <div className="sim-timeline-ticks">
          {years.map((y) => {
            const pct = ((y - startYear) / span) * 100;
            const isCurrent = !waiting && timeline.current_year === y;
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
        <span className="sim-timeline-status">{statusLabel(timeline)}</span>
        <span>{done ? "100%" : `${timeline.progress_pct.toFixed(0)}%`}</span>
      </div>
    </div>
  );
}

export default function SimulationTimeline({ telemetry, active }: Props) {
  const t = telemetry;
  const baseline: ScenarioTimeline & { status?: string } = t.baseline_timeline ?? {
    current_year: t.current_year ?? t.start_year,
    progress_pct: t.year_progress_pct,
    active: t.phase !== "idle",
    status: t.phase === "complete" ? "complete" : "running",
  };
  const intervention: ScenarioTimeline & { status?: string } = t.intervention_timeline ?? {
    current_year: t.start_year,
    progress_pct: 0,
    active: false,
    status: "waiting",
  };
  const pulse = active && (t.phase === "simulating" || t.phase === "policy");

  return (
    <div className="sim-timeline-dual">
      <SingleTimeline
        label="Baseline"
        timeline={baseline}
        startYear={t.start_year}
        stopYear={t.stop_year}
        variant="baseline"
        pulse={pulse && baseline.active}
      />
      <SingleTimeline
        label="Intervention"
        timeline={intervention}
        startYear={t.start_year}
        stopYear={t.stop_year}
        variant="intervention"
        pulse={pulse && intervention.active}
      />
    </div>
  );
}
