import type { ComorbidityCell } from "../../api/client";

interface Props {
  matrix: { title: string; cells: ComorbidityCell[] } | null;
}

export default function ComorbidityMatrix({ matrix }: Props) {
  if (!matrix?.cells.length) return null;

  const max = Math.max(...matrix.cells.map((c) => c.average), 1);

  return (
    <div className="comorbidity-matrix">
      <h4 className="viz-section-title">{matrix.title}</h4>
      <div className="comorbidity-grid">
        {matrix.cells.map((c) => (
          <div key={c.level} className="comorbidity-cell" title={`Male ${c.male.toFixed(1)}% · Female ${c.female.toFixed(1)}%`}>
            <span className="comorbidity-level">{c.label}</span>
            <div className="comorbidity-bar-track">
              <div className="comorbidity-bar-fill" style={{ width: `${(c.average / max) * 100}%` }} />
            </div>
            <span className="comorbidity-pct">{c.average.toFixed(1)}%</span>
          </div>
        ))}
      </div>
    </div>
  );
}
