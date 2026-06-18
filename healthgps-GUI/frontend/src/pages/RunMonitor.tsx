import { useEffect, useState } from "react";
import { api, type RunStatus } from "../api/client";

interface Props {
  workspaceId: string;
  polling: boolean;
  onPollingChange: (v: boolean) => void;
}

export default function RunMonitor({
  workspaceId,
  polling,
  onPollingChange,
}: Props) {
  const [status, setStatus] = useState<RunStatus | null>(null);
  const [resultsDir, setResultsDir] = useState<string | null>(null);
  const [configuredFolder, setConfiguredFolder] = useState<string | null>(null);
  const [searchedDirs, setSearchedDirs] = useState<string[]>([]);
  const [runTimestamp, setRunTimestamp] = useState<string | null>(null);
  const [files, setFiles] = useState<{ name: string; path: string }[]>([]);

  const loadLatestResults = async () => {
    const r = await api.results(workspaceId);
    setResultsDir(r.results_dir);
    setConfiguredFolder(r.configured_folder ?? null);
    setSearchedDirs(r.searched_dirs ?? []);
    setRunTimestamp(r.run_timestamp);
    setFiles(r.files.filter((f) => f.exists !== false));
  };

  useEffect(() => {
    if (!polling) return;
    const id = setInterval(async () => {
      try {
        const s = await api.runStatus(workspaceId);
        setStatus(s);
        if (s.state === "succeeded" || s.state === "failed") {
          onPollingChange(false);
          if (s.state === "succeeded") {
            await loadLatestResults();
          }
        }
      } catch {
        /* ignore poll errors */
      }
    }, 2000);
    return () => clearInterval(id);
  }, [workspaceId, polling, onPollingChange]);

  useEffect(() => {
    api.runStatus(workspaceId).then(setStatus).catch(() => {});
  }, [workspaceId]);

  const badgeClass =
    status?.state === "succeeded"
      ? "status-succeeded"
      : status?.state === "failed"
        ? "status-failed"
        : "status-running";

  return (
    <div className="run-monitor run-monitor--terminal">
      <div className="run-monitor-header">
        <h3 className="grid-card-title">Terminal output</h3>
        {status && (
          <span className={`status-badge ${badgeClass}`}>{status.state}</span>
        )}
      </div>
      <div className="log-box">{status?.log_tail || "(no log yet)"}</div>

      {status?.state === "succeeded" && (
        <div className="results-summary">
          <p className="results-heading">Latest run output</p>
          {files.length === 0 ? (
            <div className="alert alert-warning">
              No result files found on disk.
              {configuredFolder && (
                <span>
                  {" "}
                  Config says <code>{configuredFolder}</code>.
                </span>
              )}
              {searchedDirs.length > 0 && (
                <details className="results-search-details">
                  <summary>Search locations checked</summary>
                  <ul>
                    {searchedDirs.map((d) => (
                      <li key={d}>
                        <code>{d}</code>
                      </li>
                    ))}
                  </ul>
                </details>
              )}
            </div>
          ) : (
            <>
              {resultsDir && (
                <p className="results-folder">
                  <strong>Folder:</strong> {resultsDir}
                </p>
              )}
              {runTimestamp && (
                <p className="results-run-id">
                  <strong>Run:</strong> {runTimestamp}
                </p>
              )}
              <ul className="results-file-list">
                {files.map((f) => (
                  <li key={f.name}>
                    {f.name}
                    <span className="muted results-file-path">{f.path}</span>
                  </li>
                ))}
              </ul>
            </>
          )}
        </div>
      )}
    </div>
  );
}
