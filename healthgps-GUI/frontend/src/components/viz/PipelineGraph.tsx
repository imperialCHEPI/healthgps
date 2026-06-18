import type { PipelineModule } from "../../api/client";

interface Props {
  modules: PipelineModule[];
}

export default function PipelineGraph({ modules }: Props) {
  return (
    <div className="pipeline-graph" aria-label="Active module pipeline">
      {modules.map((mod, i) => (
        <div key={mod.id} className="pipeline-graph-item-wrap">
          <div
            className={`pipeline-graph-item pipeline-graph-item--${mod.status}${
              !mod.enabled ? " pipeline-graph-item--disabled" : ""
            }`}
          >
            <span className="pipeline-graph-label">{mod.label}</span>
            <span className="pipeline-graph-desc">{mod.description}</span>
          </div>
          {i < modules.length - 1 && (
            <span className="pipeline-graph-arrow" aria-hidden>
              →
            </span>
          )}
        </div>
      ))}
    </div>
  );
}
