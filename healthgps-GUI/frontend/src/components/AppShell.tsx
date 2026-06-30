import type { ReactNode } from "react";
import AppHeader from "./AppHeader";

interface Props {
  children: ReactNode;
  centered?: boolean;
  wide?: boolean;
}

export default function AppShell({
  children,
  centered = true,
  wide = false,
}: Props) {
  const pageClass = centered
    ? `page-center${wide ? " page-center--wide" : ""}`
    : "page-full";

  return (
    <>
      <AppHeader />
      <main
        className={`app-main${centered ? " app-main--centered" : " app-main--workspace"}`}
      >
        <div className={pageClass}>{children}</div>
      </main>
    </>
  );
}
