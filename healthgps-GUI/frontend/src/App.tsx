import { Route, Routes } from "react-router-dom";
import AppShell from "./components/AppShell";
import CoverPage from "./pages/CoverPage";
import ExpertUserWorkspace from "./pages/ExpertUserWorkspace";
import NewUserWizard from "./pages/NewUserWizard";
import ProjectPicker from "./pages/ProjectPicker";
import StudioWorkspace from "./pages/StudioWorkspace";

type ShellPageId = "examples" | "new-user" | "expert";

function ShellPage({ page }: { page: ShellPageId }) {
  if (page === "examples") {
    return (
      <AppShell centered={true} wide={true}>
        <ProjectPicker />
      </AppShell>
    );
  }
  if (page === "new-user") {
    return (
      <AppShell centered={false} wide={true}>
        <NewUserWizard />
      </AppShell>
    );
  }
  return (
    <AppShell centered={true} wide={false}>
      <ExpertUserWorkspace />
    </AppShell>
  );
}

export default function App() {
  return (
    <Routes>
      <Route path="/" element={<CoverPage />} />
      <Route path="/examples" element={<ShellPage page="examples" />} />
      <Route path="/new-user" element={<ShellPage page="new-user" />} />
      <Route path="/expert" element={<ShellPage page="expert" />} />
      <Route
        path="/workspace/new/:projectId"
        element={
          <AppShell centered={false}>
            <StudioWorkspace />
          </AppShell>
        }
      />
      <Route
        path="/workspace/:workspaceId"
        element={
          <AppShell centered={false}>
            <StudioWorkspace />
          </AppShell>
        }
      />
    </Routes>
  );
}
