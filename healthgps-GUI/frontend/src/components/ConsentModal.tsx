import { useState } from "react";

interface Props {
  open: boolean;
  command: string;
  actionLabel: string;
  onConfirm: (skipSession: boolean) => void;
  onCancel: () => void;
}

export default function ConsentModal({
  open,
  command,
  actionLabel,
  onConfirm,
  onCancel,
}: Props) {
  const [checked, setChecked] = useState(false);
  const [skipSession, setSkipSession] = useState(false);

  if (!open) return null;

  return (
    <div className="modal-overlay" role="dialog" aria-modal="true">
      <div className="modal">
        <h2>Terminal access required</h2>
        <p>
          HealthGPS Studio will run <strong>HealthGPS.Console</strong> on your
          machine using the config file below. Output appears in the Run monitor
          on this page. First run may download data.
        </p>
        <p>
          <strong>Command:</strong>
        </p>
        <pre>{command || "(loading…)"}</pre>
        <label className="consent">
          <input
            type="checkbox"
            checked={checked}
            onChange={(e) => setChecked(e.target.checked)}
          />
          <span>I understand and allow HealthGPS.Console to run locally</span>
        </label>
        <label className="consent">
          <input
            type="checkbox"
            checked={skipSession}
            onChange={(e) => setSkipSession(e.target.checked)}
          />
          <span>Don&apos;t ask again this session</span>
        </label>
        <div className="btn-row">
          <button className="secondary" onClick={onCancel}>
            Cancel
          </button>
          <button
            className="primary"
            disabled={!checked}
            onClick={() => onConfirm(skipSession)}
          >
            {actionLabel}
          </button>
        </div>
      </div>
    </div>
  );
}
