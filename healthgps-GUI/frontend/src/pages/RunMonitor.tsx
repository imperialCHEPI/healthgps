import { useCallback, useEffect, useState } from "react";
import { api, type RunStatus } from "../api/client";

interface Props {
  workspaceId: string;
  polling: boolean;
  onPollingChange: (v: boolean) => void;
  onRunComplete?: () => void;
  collapsed?: boolean;
}

export default function RunMonitor({
  workspaceId,
  polling: _polling,
  onPollingChange,
  onRunComplete,
  collapsed = false,
}: Props) {
  const [status, setStatus] = useState<RunStatus | null>(null);
  const [expanded, setExpanded] = useState(false);
  const [resultsDir, setResultsDir] = useState<string | null>(null);
  const [configuredFolder, setConfiguredFolder] = useState<string | null>(null);
  const [searchedDirs, setSearchedDirs] = useState<string[]>([]);
  const [runTimestamp, setRunTimestamp] = useState<string | null>(null);
  const [files, setFiles] = useState<{ name: string; path: string }[]>([]);

  const loadLatestResults = useCallback(async () => {
    const r = await api.results(workspaceId);
    setResultsDir(r.results_dir);
    setConfiguredFolder(r.configured_folder ?? null);
    setSearchedDirs(r.searched_dirs ?? []);
    setRunTimestamp(r.run_timestamp);
    setFiles(r.files.filter((f) => f.exists !== false));
  }, [workspaceId]);

  const handleTerminalState = useCallback(
    async (s: RunStatus) => {
      setStatus(s);
      if (s.state === "succeeded" || s.state === "failed") {
        onPollingChange(false);
        if (s.state === "succeeded") {
          await loadLatestResults();
          onRunComplete?.();
        }
      }
    },
    [loadLatestResults, onPollingChange, onRunComplete]
  );

  useEffect(() => {
    let cancelled = false;
    const poll = async () => {
      try {
        const s = await api.runStatus(workspaceId);
        if (!cancelled) await handleTerminalState(s);
      } catch {
        /* ignore poll errors */
      }
    };
    poll();
    const id = window.setInterval(poll, 2000);
    return () => {
      cancelled = true;
      window.clearInterval(id);
    };
  }, [workspaceId, handleTerminalState]);

  const badgeClass =
    status?.state === "succeeded"
      ? "status-succeeded"
      : status?.state === "failed"
        ? "status-failed"
        : status?.state === "running"
          ? "status-running"
          : "";

  const isDone = status?.state === "succeeded" || status?.state === "failed";
  const isCollapsed = collapsed && isDone && !expanded;
  const jsonFile = files.find(
    (f) => f.name.startsWith("HealthGPS_Result") && f.name.endsWith(".json")
  );

  return (
    <div
      className={`run-monitor run-monitor--terminal${
        isCollapsed ? " run-monitor--collapsed" : ""
      }${isDone && collapsed ? " run-monitor--disabled" : ""}`}
    >
      <div className="run-monitor-header">
        <h3 className="grid-card-title">Terminal output</h3>
        <div className="run-monitor-header-actions">
          {status && (
            <span className={`status-badge ${badgeClass}`}>{status.state}</span>
          )}
          {collapsed && isDone && (
            <button
              type="button"
              className="secondary run-monitor-toggle"
              onClick={() => setExpanded((v) => !v)}
            >
              {expanded ? "Collapse" : "Show log"}
            </button>
          )}
        </div>
      </div>

      {isCollapsed ? (
        <p className="run-monitor-collapsed-msg muted">
          Run complete — visualisations are ready above.
          {jsonFile && (
            <>
              {" "}
              Result: <code>{jsonFile.name}</code>
            </>
          )}
        </p>
      ) : (
        <>
          <div className={`log-box${isDone && collapsed ? " log-box--readonly" : ""}`}>
            {status?.log_tail || "(no log yet)"}
          </div>

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
        </>
      )}
    </div>
  );
}
