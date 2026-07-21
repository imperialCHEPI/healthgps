# Windows MSVC / Ninja build troubleshooting

**Author:** Mahima Ghosh
**Last updated:** July 2026
**Engineering contact:** Mahima Ghosh — please reach out if you hit a related build failure or anything not covered here.

## About this note

I wrote this after a Windows Visual Studio / CMake / Ninja build that had worked for me for weeks suddenly stopped configuring and compiling. Nothing in the Health-GPS source had changed — the failure was entirely in the local MSVC toolchain environment. I am documenting what I saw, how I diagnosed it, and what I recommend we do next time so the team does not lose a day on the same trap.

This is **not** a coding defect in Health-GPS. The project CMake presets and source remain valid; the break was a corrupted or incomplete Visual Studio C++ toolset install, combined with Ninja needing a fully initialised MSVC developer environment.

### Related documentation

| Topic | Document |
| ----- | -------- |
| Normal build instructions | [Developer Guide — Building from source](../../developer/development.md) |
| Technical developer notes | [Technical developer index](README.md) |
| Formal developer docs | [developer/](../../developer/) |
| All technical docs | [Technical documentation index](../README.md) |
| Documentation home | [documentation/index.md](../../index.md) |
| Quick start (binaries) | [Quick Start](../../user/getstarted.md) |

---

## Table of contents

1. [Symptoms I hit](#1-symptoms-i-hit)
2. [Root causes](#2-root-causes)
3. [How I diagnosed it](#3-how-i-diagnosed-it)
4. [What fixed it](#4-what-fixed-it)
5. [What to do next time (checklist)](#5-what-to-do-next-time-checklist)
6. [Permanent repair of Visual Studio](#6-permanent-repair-of-visual-studio)
7. [What is not the problem](#7-what-is-not-the-problem)

---

## 1. Symptoms I hit

I saw the failure evolve through three related errors. They look different in the CMake output window, but they share the same underlying toolchain problem.

### Error A — missing standard header

```text
fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
```

(Same pattern for `<algorithm>`, `<functional>`, `<optional>`, and other standard headers.)

This appeared when compiling `model_parser.cpp` / including `risk_factor_model.h`. It is easy to misread as a project include-path bug. It is not.

### Error B — CMake cannot find the compiler

```text
-- The CXX compiler identification is unknown
CMake Error: No CMAKE_CXX_COMPILER could be found.
```

Here Visual Studio launched CMake **without** a usable MSVC developer environment (`INCLUDE` / `PATH` for `cl.exe` missing).

### Error C — compiler found, link of the CMake test program fails

```text
-- The CXX compiler identification is MSVC 19.42.x
-- Check for working CXX compiler: ...\14.42.34433\...\cl.exe - broken
LINK : fatal error LNK1104: cannot open file 'MSVCRTD.lib'
```

This was the decisive clue. CMake could find `cl.exe`, but the **x64 runtime libraries for that toolset were missing**.

---

## 2. Root causes

### 2.1 Ninja + MSVC needs the developer environment

Health-GPS uses the **Ninja** generator on Windows (`CMakePresets.json`). With Ninja, MSVC does not bake standard include/lib paths into every compile command the way a Visual Studio generator sometimes appears to. The linker and compiler rely on environment variables set by `vcvars64.bat` (or Visual Studio’s equivalent):

- `INCLUDE` — standard headers (`cstdint`, etc.)
- `LIB` — runtime import libraries (`MSVCRTD.lib`, etc.)
- `PATH` — `cl.exe`, `link.exe`

If CMake/Ninja runs without that environment, you get Error A or Error B.

### 2.2 A broken / incomplete MSVC toolset on disk

On my machine I had multiple MSVC toolsets installed under:

`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\`

| Toolset | x64 `MSVCRTD.lib` |
| ------- | ----------------- |
| **14.42.34433** | **Missing** (broken install — `lib\x64` empty or incomplete) |
| **14.44.35207** | Present (healthy) |

Visual Studio / CMake selected **14.42**, so linking failed with `LNK1104` even though a good **14.44** toolset was already installed. That matches a partial Visual Studio update leaving an older toolset in a bad state while still offering it as a default.

vcpkg was a red herring: it correctly found a compiler binary, but that does not mean the active toolset’s **libs** are intact.

---

## 3. How I diagnosed it

1. Confirmed the Health-GPS source includes were fine (`#include <cstdint>` is correct).
2. Compared CMake environment dumps:
   - First run: no `INCLUDE` / `LIB` → Error B / A.
   - Later run (from Native Tools): `INCLUDE` present, but `LIB` pointed at the **14.42** tree without MSVC x64 libs → Error C.
3. Checked on disk:

```bat
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\lib\x64\msvcrtd.lib"
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64\msvcrtd.lib"
```

1. Forced the healthy toolset:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44.35207
```

After that, `LIB` included `...\MSVC\14.44.35207\lib\x64`, CMake configured cleanly, and the release build completed.

---

## 4. What fixed it

No Health-GPS code or CMake preset changes were required.

### Immediate recovery (what I used)

1. Close Visual Studio.
2. Open **x64 Native Tools Command Prompt for VS 2022**.
3. Initialise the **working** toolset explicitly (use the version that has `lib\x64\msvcrtd.lib` on your PC — on mine that was `14.44.35207`):

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44.35207
cd /d C:\HealthGPS
```

1. Delete the stale CMake cache for the preset (optional but safest after a broken configure):

```bat
rmdir /s /q out\build\windows-release
```

1. Reconfigure and build:

```bat
cmake --preset windows-release
cmake --build out\build\windows-release
```

1. To continue in the IDE with the same environment:

```bat
devenv C:\HealthGPS
```

Then select the **windows-release** configuration and **Build → Build All**. Prefer not to “Delete Cache and Reconfigure” from a plain `devenv` launch that still defaults to the broken 14.42 toolset.

### Expected success signals

```text
-- The CXX compiler identification is MSVC ...
-- Check for working CXX compiler: ...\14.44.35207\...\cl.exe - skipped
-- Configuring done
-- Generating done
```

Build output should produce:

`out\build\windows-release\src\HealthGPS.Console\HealthGPS.Console.exe`

---

## 5. What to do next time (checklist)

If Windows configure/build fails again, I recommend this order:

| Step | Action |
| ---- | ------ |
| 1 | Confirm it is **not** a source change — does a clean clone of `main` fail the same way? |
| 2 | Open **x64 Native Tools Command Prompt** (not a plain PowerShell/Cursor terminal). |
| 3 | Check `echo %INCLUDE%` and `echo %LIB%` — both must be non-empty and include an `MSVC\14.xx...\lib\x64` path in `LIB`. |
| 4 | Verify `msvcrtd.lib` exists under that `lib\x64` folder. |
| 5 | If missing, list other toolsets under `VC\Tools\MSVC\` and retry with `-vcvars_ver=<good-version>`. |
| 6 | Delete `out\build\windows-release` (or the preset folder you use) and reconfigure. |
| 7 | If multiple toolsets are broken, run **Visual Studio Installer → Repair** (see [§6](#6-permanent-repair-of-visual-studio)). |
| 8 | Only after the toolchain works, revisit project CMake or source — almost always unnecessary for these errors. |

### Quick commands

```bat
:: Which toolsets exist?
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"

:: Does the active toolset have the debug CRT import lib?
where /R "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC" msvcrtd.lib
```

---

## 6. Permanent repair of Visual Studio

The `-vcvars_ver=` workaround is fine for a day, but I prefer a clean install so Visual Studio defaults to a healthy toolset again.

1. Open **Visual Studio Installer**.
2. Select **Visual Studio Community 2022** → **Repair**.
3. If the problem remains, **Modify** → **Individual components**:
   - Ensure **MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)** is installed.
   - Ensure a **Windows 10/11 SDK** is installed.
   - Optionally remove clearly broken older MSVC 14.42 (or similar) components if Repair left them incomplete.
4. Reboot if prompted.
5. Open a fresh **x64 Native Tools** prompt, confirm `LIB` points at a toolset whose `lib\x64\msvcrtd.lib` exists, then reconfigure Health-GPS.

After a successful Repair, opening the folder in Visual Studio normally should work again without the explicit `-vcvars_ver` flag.

---

## 7. What is not the problem

| Red herring | Why I ruled it out |
| ----------- | ------------------ |
| Health-GPS `#include <cstdint>` | Standard and correct; fails only when MSVC headers are invisible. |
| CMakePresets “wrong” | Presets were fine; the environment / toolset was not. |
| vcpkg registry warnings | Noise; packages installed successfully. |
| Missing project code | Full release build succeeded once the healthy 14.44 toolset was active. |

---

## Questions?

If you see `cstdint`, `CMAKE_CXX_COMPILER`, or `MSVCRTD.lib` errors on Windows, start with this note and the [Developer Guide](../../developer/development.md). If your machine shows a different broken toolset version, contact **Mahima Ghosh** and we can extend this document with that case.

---

*July 2026 — Windows MSVC / Ninja environment failure on the Health-GPS development machine; resolved by switching to a complete MSVC 14.44 toolset and repairing Visual Studio.*

[← Technical developer notes](README.md) · [← Technical documentation index](../README.md) · [Documentation home](../../index.md)
