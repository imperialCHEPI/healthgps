import { Component, type ErrorInfo, type ReactNode } from "react";

interface Props {
  children: ReactNode;
}

interface State {
  error: Error | null;
}

export default class ErrorBoundary extends Component<Props, State> {
  state: State = { error: null };

  static getDerivedStateFromError(error: Error): State {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error("HealthGPS Studio render error:", error, info);
  }

  render() {
    if (this.state.error) {
      return (
        <div className="error-boundary">
          <h2>Something went wrong</h2>
          <p className="muted">{this.state.error.message}</p>
          <button
            type="button"
            className="primary"
            onClick={() => {
              this.setState({ error: null });
              window.location.href = "/";
            }}
          >
            Back to home
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}
