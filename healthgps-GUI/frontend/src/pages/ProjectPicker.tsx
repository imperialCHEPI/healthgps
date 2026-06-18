import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { api, type CatalogProgram } from "../api/client";

export default function ProjectPicker() {
  const [programs, setPrograms] = useState<CatalogProgram[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [offline, setOffline] = useState(false);
  const navigate = useNavigate();

  useEffect(() => {
    api
      .catalog()
      .then((data) => {
        setPrograms(data.programs);
        setOffline(data.offline);
      })
      .catch((e: Error) => setError(e.message))
      .finally(() => setLoading(false));
  }, []);

  if (loading) return <p className="muted">Loading catalog…</p>;
  if (error) {
    return (
      <div className="alert alert-error">
        Could not load catalog. Is the backend running? {error}
      </div>
    );
  }

  const active = programs.filter((p) => p.status === "active");
  const upcoming = programs.filter((p) => p.status === "upcoming");

  return (
    <div className="catalog-page">
      <button type="button" className="secondary catalog-back" onClick={() => navigate("/")}>
        ← Home
      </button>
      {offline && (
        <div className="alert alert-warning offline-banner">
          Backend offline — showing built-in programme list. Start the API for
          live data and runs.
        </div>
      )}
      <div className="page-intro">
        <h2>Select project and country</h2>
        <p className="muted">
          Choose an active programme, then pick the country dataset to configure
          and run locally.
        </p>
      </div>

      <section className="catalog-section">
        <h3 className="catalog-section-title">Active programmes</h3>
        <div className="program-grid">
          {active.map((program) => (
            <article key={program.id} className="program-card program-card--active">
              <header className="program-card-header">
                <div>
                  <h4>{program.name}</h4>
                  {program.subtitle && (
                    <p className="program-subtitle">{program.subtitle}</p>
                  )}
                </div>
                <span className="badge badge-active">Available</span>
              </header>
              <div className="country-row">
                <span className="country-row-label">Countries / data</span>
                <div className="country-chips">
                  {program.countries.map((c) => (
                    <button
                      key={`${program.id}-${c.id}`}
                      type="button"
                      className="country-chip country-chip--active"
                      disabled={!c.available || !c.project_id}
                      onClick={() =>
                        c.project_id && navigate(`/workspace/new/${c.project_id}`)
                      }
                      title={
                        c.example_path
                          ? c.example_path
                          : "Example data not found"
                      }
                    >
                      {c.name}
                    </button>
                  ))}
                </div>
              </div>
            </article>
          ))}
        </div>
      </section>

      <section className="catalog-section catalog-section--upcoming">
        <h3 className="catalog-section-title">Upcoming programmes</h3>
        <div className="program-grid program-grid--upcoming">
          {upcoming.map((program) => (
            <article
              key={program.id}
              className="program-card program-card--upcoming"
            >
              <header className="program-card-header">
                <div>
                  <h4>{program.name}</h4>
                  <p className="program-subtitle">Upcoming — not yet available</p>
                </div>
                <span className="badge badge-upcoming">Upcoming</span>
              </header>
              <div className="country-row">
                <span className="country-row-label">Planned countries</span>
                <div className="country-chips">
                  {program.countries.map((c) => (
                    <span
                      key={`${program.id}-${c.id}`}
                      className="country-chip country-chip--disabled"
                    >
                      {c.name}
                    </span>
                  ))}
                </div>
              </div>
            </article>
          ))}
        </div>
      </section>
    </div>
  );
}
