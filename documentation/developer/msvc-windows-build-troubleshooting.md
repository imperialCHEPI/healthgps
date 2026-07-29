# Windows MSVC / Ninja build troubleshooting

## Global Health Policy Simulation model

| [Home](../index.md) | [Quick Start](../user/getstarted.md) | [User Guide](../user/userguide.md) | [Schemas](../user/schemas.md) | [Models](../user/models-overview.md) | [Architecture](architecture.md) | [Data Model](datamodel.md) | [Developer Guide](development.md) | [Technical docs](../technical/README.md) | [API](https://imperialchepi.github.io/healthgps/api/) |

**Last updated:** July 2026

## Why I wrote this

A Windows Visual Studio build that had been fine for weeks suddenly failed on configure and compile. Nothing in the Health-GPS source had changed. The problem was the local MSVC toolchain: a broken toolset install, plus Ninja needing a proper developer environment.

This is not a bug in Health-GPS. The presets and code were fine. I am writing down what went wrong and how to get unstuck so nobody on the team wastes another afternoon on it.

### Related documentation

| Topic                      | Document                                      |
| -------------------------- | --------------------------------------------- |
| Normal build steps         | [Developer Guide](development.md)             |
| Developer docs             | [developer/README.md](README.md)              |
| Technical guides and plans | [technical/README.md](../technical/README.md) |
| Documentation index        | [documentation/README.md](../README.md)       |
| Quick start (binaries)     | [Quick Start](../user/getstarted.md)          |

---

## Table of contents

1. [Symptoms](#1-symptoms)
2. [Root causes](#2-root-causes)
3. [How I diagnosed it](#3-how-i-diagnosed-it)
4. [What fixed it](#4-what-fixed-it)
5. [Checklist for next time](#5-checklist-for-next-time)
6. [Repairing Visual Studio properly](#6-repairing-visual-studio-properly)
7. [What it was not](#7-what-it-was-not)

---

## 1. Symptoms

I hit three related errors. They look different in the CMake output, but they are the same class of problem.

### Error A: missing standard header

```text
fatal error C1083: Cannot open include file: 'cstdint': No such file or directory
```

Same idea for `<algorithm>`, `<functional>`, `<optional>`, and friends. Easy to blame a project include path. That was a red herring.

### Error B: CMake cannot find the compiler

```text
-- The CXX compiler identification is unknown
CMake Error: No CMAKE_CXX_COMPILER could be found.
```

Visual Studio ran CMake without a usable MSVC environment (`INCLUDE` / `PATH` for `cl.exe` missing).

### Error C: compiler found, CMake test link fails

```text
-- The CXX compiler identification is MSVC 19.42.x
-- Check for working CXX compiler: ...\14.42.34433\...\cl.exe - broken
LINK : fatal error LNK1104: cannot open file 'MSVCRTD.lib'
```

This one was the giveaway. `cl.exe` was on disk, but the x64 runtime libs for that toolset were missing.

---

## 2. Root causes

### 2.1 Ninja needs the MSVC developer environment

Health-GPS uses the Ninja generator on Windows (`CMakePresets.json`). With Ninja, MSVC leans on environment variables from `vcvars64.bat` (or Visual Studio's equivalent):

- `INCLUDE`: standard headers (`cstdint`, etc.)
- `LIB`: runtime import libraries (`MSVCRTD.lib`, etc.)
- `PATH`: `cl.exe`, `link.exe`

Without that environment you get Error A or Error B.

### 2.2 Broken toolset on disk

I had more than one MSVC toolset under:

`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\`

| Toolset         | x64 `MSVCRTD.lib`                           |
| --------------- | ------------------------------------------- |
| **14.42.34433** | **Missing** (`lib\x64` empty or incomplete) |
| **14.44.35207** | Present                                     |

Visual Studio / CMake preferred **14.42**, so linking failed even though **14.44** was fine. That fits a partial VS update leaving an older toolset half-installed.

vcpkg also found a compiler binary. That does not mean the active toolset's libs are intact.

---

## 3. How I diagnosed it

1. Confirmed `#include <cstdint>` in our headers is normal and correct.
2. Compared CMake environment dumps:
   - First run: no `INCLUDE` / `LIB` -> Errors B / A.
   - Later run (from Native Tools): `INCLUDE` present, but `LIB` pointed at the broken 14.42 tree -> Error C.
3. Checked on disk:

```bat
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\lib\x64\msvcrtd.lib"
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64\msvcrtd.lib"
```

1. Forced the healthy toolset:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44.35207
```

After that, `LIB` included `...\MSVC\14.44.35207\lib\x64`, CMake configured, and the release build completed.

---

## 4. What fixed it

No Health-GPS code or CMake preset edits were needed.

### Immediate recovery

1. Close Visual Studio.
2. Open **x64 Native Tools Command Prompt for VS 2022**.
3. Initialise the toolset that actually has `lib\x64\msvcrtd.lib` (on my machine: `14.44.35207`):

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44.35207
cd /d C:\HealthGPS
```

1. Clear a stale configure cache if needed:

```bat
rmdir /s /q out\build\windows-release
```

1. Reconfigure and build:

```bat
cmake --preset windows-release
cmake --build out\build\windows-release
```

1. To keep working in the IDE with the same environment:

```bat
devenv C:\HealthGPS
```

Then pick **windows-release** and **Build -> Build All**. Avoid "Delete Cache and Reconfigure" from a normal `devenv` launch that still defaults to the broken 14.42 toolset.

### What success looks like

```text
-- The CXX compiler identification is MSVC ...
-- Check for working CXX compiler: ...\14.44.35207\...\cl.exe - skipped
-- Configuring done
-- Generating done
```

Binary:

`out\build\windows-release\src\HealthGPS.Console\HealthGPS.Console.exe`

---

## 5. Checklist for next time

| Step | Action                                                                                                                 |
| ---- | ---------------------------------------------------------------------------------------------------------------------- |
| 1    | Confirm it is not a source change. Does a clean clone of `main` fail the same way?                                     |
| 2    | Use **x64 Native Tools Command Prompt**, not a plain PowerShell or terminal.                                           |
| 3    | Check `echo %INCLUDE%` and `echo %LIB%`. Both should be set, and `LIB` should include an `MSVC\14.xx...\lib\x64` path. |
| 4    | Confirm `msvcrtd.lib` exists under that `lib\x64` folder.                                                              |
| 5    | If missing, list other toolsets under `VC\Tools\MSVC\` and retry with `-vcvars_ver=<good-version>`.                    |
| 6    | Delete `out\build\windows-release` (or your preset folder) and reconfigure.                                            |
| 7    | If several toolsets look broken, run Visual Studio Installer -> Repair (section 6).                                    |
| 8    | Only after the toolchain works should you dig into project CMake or source. Usually you will not need to.              |

### Handy commands

```bat
:: Which toolsets exist?
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"

:: Does any toolset have the debug CRT import lib?
where /R "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC" msvcrtd.lib
```

---

## 6. Repairing Visual Studio properly

`-vcvars_ver=` is fine for a day. I still prefer a clean install so Visual Studio defaults to a healthy toolset.

1. Open **Visual Studio Installer**.
2. Select **Visual Studio Community 2022** -> **Repair**.
3. If it still fails, **Modify** -> **Individual components**:
   - Install **MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)**.
   - Install a **Windows 10/11 SDK**.
   - Optionally remove a clearly broken older 14.42 (or similar) component if Repair left it incomplete.
4. Reboot if prompted.
5. Open a fresh **x64 Native Tools** prompt, confirm `LIB` points at a toolset with `lib\x64\msvcrtd.lib`, then reconfigure Health-GPS.

After a good Repair, opening the folder in Visual Studio normally should work again without `-vcvars_ver`.

---

## 7. What it was not

| Suspect                         | Why I ruled it out                                   |
| ------------------------------- | ---------------------------------------------------- |
| Health-GPS `#include <cstdint>` | Correct; only fails when MSVC headers are invisible. |
| Broken CMakePresets             | Presets were fine; environment / toolset was not.    |
| vcpkg registry warnings         | Noise; packages installed successfully.              |
| Missing project code            | Full release build worked once 14.44 was active.     |

---

## Questions?

If you see `cstdint`, `CMAKE_CXX_COMPILER`, or `MSVCRTD.lib` errors on Windows, start here and with the [Developer Guide](development.md). If your machine shows a different broken toolset version, contact me (**Mahima Ghosh**) and we can note that case here too.

---

*July 2026. Windows MSVC / Ninja environment failure on my Health-GPS machine; fixed by switching to a complete MSVC 14.44 toolset and repairing Visual Studio.*

---

**Author:** Mahima Ghosh
