import type { SchemaValidationError } from "../../api/client";

interface Props {
  valid: boolean;
  errors: string[];
  details?: SchemaValidationError[];
}

export default function SchemaValidationBanner({ valid, errors, details }: Props) {
  if (valid && errors.length === 0) return null;

  const rows = details?.length ? details : errors.map((e) => ({ summary: e, field: "", message: e, validator: "", expected: null, supplied: null }));

  return (
    <div className={`schema-banner${valid ? "" : " schema-banner--error"}`}>
      <strong>{valid ? "Schema warnings" : "Schema validation failed"}</strong>
      <p className="schema-banner-hint">
        Fix these before compute runs. Fields show expected vs supplied values from the JSON schema.
      </p>
      <ul className="schema-banner-list">
        {rows.map((row, i) => (
          <li key={`${row.field}-${i}`} className="schema-banner-row">
            <code className="schema-banner-field">{row.field || "(root)"}</code>
            <span className="schema-banner-message">{row.message || row.summary}</span>
            {(row.expected != null || row.supplied != null) && (
              <div className="schema-banner-values">
                {row.expected != null && (
                  <span>
                    <em>Expected:</em> <code>{row.expected}</code>
                  </span>
                )}
                {row.supplied != null && (
                  <span>
                    <em>Supplied:</em> <code>{row.supplied}</code>
                  </span>
                )}
              </div>
            )}
          </li>
        ))}
      </ul>
    </div>
  );
}
