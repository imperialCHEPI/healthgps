import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { api, type CountryOption } from "../api/client";

export default function ExpertUserWorkspace() {
  const navigate = useNavigate();
  const [countries, setCountries] = useState<CountryOption[]>([]);
  const [countryId, setCountryId] = useState("");
  const [sessionLabel, setSessionLabel] = useState("");
  const [configFile, setConfigFile] = useState<File | null>(null);
  const [dataFiles, setDataFiles] = useState<FileList | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    api
      .countries()
      .then((list) => {
        setCountries(list);
        if (list.length > 0) setCountryId(list[0].id);
      })
      .catch((e: Error) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  const selectedCountry = countries.find((c) => c.id === countryId);

  const submit = async () => {
    if (!configFile || !selectedCountry) {
      setError("Please select a country and upload a config.json file.");
      return;
    }
    setBusy(true);
    setError(null);
    try {
      const meta = await api.createExpertSession({
        country_id: selectedCountry.id,
        country_name: selectedCountry.name,
        session_label: sessionLabel || selectedCountry.name,
        config_file: configFile,
        data_files: dataFiles ? Array.from(dataFiles) : [],
      });
      navigate(`/workspace/${meta.id}`);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  };

  if (loading) {
    return (
      <div className="wizard-page wizard-page--centered">
        <p className="muted">Loading…</p>
      </div>
    );
  }

  return (
    <div className="wizard-page wizard-page--centered">
      <div className="wizard-header">
        <button type="button" className="secondary" onClick={() => navigate("/")}>
          ← Home
        </button>
        <div>
          <h2>Expert user — bring your own data</h2>
          <p className="muted">
            Upload a HealthGPS config and supporting data files for any country.
          </p>
        </div>
      </div>

      {error && <div className="alert alert-error">{error}</div>}

      <div className="expert-form grid-card">
        <div className="field">
          <label>Country</label>
          <select value={countryId} onChange={(e) => setCountryId(e.target.value)}>
            {countries.map((c) => (
              <option key={c.id} value={c.id}>
                {c.name}
              </option>
            ))}
          </select>
        </div>

        <div className="field">
          <label>Session label (optional)</label>
          <input
            type="text"
            placeholder={selectedCountry?.name ?? "My project"}
            value={sessionLabel}
            onChange={(e) => setSessionLabel(e.target.value)}
          />
        </div>

        <div className="field">
          <label>Config file (required)</label>
          <input
            type="file"
            accept=".json,application/json"
            onChange={(e) => setConfigFile(e.target.files?.[0] ?? null)}
          />
          <span className="muted field-hint">
            Your HealthGPS config.json — paths inside should resolve relative to
            the uploaded data folder.
          </span>
        </div>

        <div className="field">
          <label>Additional data files (optional)</label>
          <input
            type="file"
            multiple
            onChange={(e) => setDataFiles(e.target.files)}
          />
          <span className="muted field-hint">
            CSV, JSON, or other inputs referenced by your config.
          </span>
        </div>

        <div className="wizard-actions">
          <button
            type="button"
            className="primary"
            disabled={busy || !configFile}
            onClick={submit}
          >
            {busy ? "Uploading…" : "Create expert session"}
          </button>
        </div>
      </div>
    </div>
  );
}
