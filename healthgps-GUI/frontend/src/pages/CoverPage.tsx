import { useNavigate } from "react-router-dom";

export default function CoverPage() {
  const navigate = useNavigate();

  return (
    <div className="cover-page">
      <div className="cover-content cover-content--visible">
        <div className="cover-brand">
          <img
            src="/healthgps-logo.png"
            alt="Health-GPS"
            className="cover-brand-logo"
          />
          <span className="cover-brand-studio">Studio</span>
        </div>

        <h1 className="cover-title">
          Welcome to HealthGPS microsimulation tool
        </h1>
        <p className="cover-subtitle muted">
          Build virtual populations, test policies, and explore health outcomes —
          choose how you want to begin.
        </p>

        <div className="cover-options cover-options--vertical">
          <button
            type="button"
            className="cover-option"
            onClick={() => navigate("/new-user")}
          >
            <span className="cover-option-icon">✦</span>
            <span className="cover-option-text">
              <span className="cover-option-label">New user</span>
              <span className="cover-option-desc">
                Configure a population from scratch — country, demographics, risk
                factors, and socioeconomic settings.
              </span>
            </span>
          </button>

          <button
            type="button"
            className="cover-option"
            onClick={() => navigate("/expert")}
          >
            <span className="cover-option-icon">⬆</span>
            <span className="cover-option-text">
              <span className="cover-option-label">Expert user</span>
              <span className="cover-option-desc">
                Upload your own config and data files for any country you are
                modelling.
              </span>
            </span>
          </button>

          <button
            type="button"
            className="cover-option cover-option--accent"
            onClick={() => navigate("/examples")}
          >
            <span className="cover-option-icon">◎</span>
            <span className="cover-option-text">
              <span className="cover-option-label">Use our existing examples</span>
              <span className="cover-option-desc">
                Open curated programme datasets — STOP, FINCH, India, and more.
              </span>
            </span>
          </button>
        </div>
      </div>
    </div>
  );
}
