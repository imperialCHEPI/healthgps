# Technical developer notes

Build, tooling, and environment notes that sit alongside the formal [Developer Guide](../../developer/development.md). These documents capture issues I (Mahima) have hit in practice and how to recover without changing project source.

**Engineering contact:** Mahima Ghosh — reach out for Windows/MSVC, CMake, or vcpkg problems.

---

## Documents

| Document | Description |
| -------- | ----------- |
| [Windows MSVC / Ninja build troubleshooting](msvc-windows-build-troubleshooting.md) | Missing standard headers (`cstdint`), broken `MSVCRTD.lib`, toolset mismatch — diagnosis and recovery |

### Related clusters

| If you are working on… | Start with… | Then see… |
| ---------------------- | ----------- | --------- |
| Building Health-GPS on Windows | [Developer Guide — Building from source](../../developer/development.md) | [MSVC troubleshooting](msvc-windows-build-troubleshooting.md) |
| CMake presets / vcpkg | [Developer Guide](../../developer/development.md) | [Technical index](../README.md) |
| Architecture / modules | [Software Architecture](../../developer/architecture.md) | [Update report](../guides/healthgps-update-report-2026-02-20.md) |

---

## Documentation map

| Area | Index |
| ---- | ----- |
| Documentation home | [documentation/index.md](../../index.md) |
| User docs | [user/](../../user/) |
| Formal developer docs | [developer/](../../developer/) |
| Technical guides & plans | [technical/](../README.md) |

---

[← Technical documentation index](../README.md) · [Documentation home](../../index.md) · **Maintainer:** Mahima Ghosh
