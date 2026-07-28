# GitHub Pages docs deploy troubleshooting

**Author:** Mahima Ghosh

**Last updated:** July 2026

## Why this exists

The public site ([imperialchepi.github.io/healthgps](https://imperialchepi.github.io/healthgps/)) is built by the **Deploy Jekyll and Doxygen** workflow (`.github/workflows/docs.yml`). It does **not** run on every push to `main`. It runs on:

- a published **release**, or
- a manual **workflow_dispatch** from the Actions tab

Markdown/Jekyll and the Doxygen API are built in the same job. If CMake configure fails, the whole deploy fails — including the markdown site — even when Jekyll itself succeeded.

### Related documentation

| Topic | Document |
| ----- | -------- |
| Local API build | [Developer Guide](development.md#building-api-documentation) |
| Docs layout / API note | [documentation/README.md](../README.md) |
| Workflow file | [`.github/workflows/docs.yml`](../../.github/workflows/docs.yml) |
| CI reference (vcpkg cache) | [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) |

---

## Table of contents

1. [Symptoms](#1-symptoms)
2. [What usually went wrong](#2-what-usually-went-wrong)
3. [What to do](#3-what-to-do)
4. [Checklist](#4-checklist)

---

## 1. Symptoms

On GitHub → **Actions** → **Deploy Jekyll and Doxygen**:

| Step | Typical result when this issue hits |
| ---- | ----------------------------------- |
| Build with Jekyll | Success |
| Install VCPKG | Success |
| **Configure HealthGPS** | **Failure** (exit code 1) |
| Build with doxygen | Skipped |
| Upload artifact / deploy | Skipped |

The annotation often only says `Process completed with exit code 1` on the Configure step. Open that step’s log for the real CMake/vcpkg error.

The live Pages site keeps the **previous** successful deploy until a new run succeeds.

---

## 2. What usually went wrong

The docs job configures the C++ project with:

```sh
cmake --preset=linux-release -DBUILD_DOC=ON
```

so Doxygen can build `/api`. That needs a working **vcpkg** install on the runner.

A common failure mode is the docs workflow using a **different vcpkg binary-cache setup** than CI:

| Setting | Should match CI |
| ------- | --------------- |
| `VCPKG_BINARY_SOURCES` | `clear;x-gha,readwrite` |
| Cache env export | `ACTIONS_CACHE_URL` and `ACTIONS_RUNTIME_TOKEN` via `actions/github-script` |

Older docs workflows used `clear;nuget,GitHub,readwrite` without the GHA cache export. That can make Configure fail even though the markdown content is fine.

Other real causes of the same step failing:

- vcpkg / dependency / toolchain drift vs `CMakePresets.json` and the manifest
- Missing apt packages (`build-essential`, `ninja-build`, `doxygen`, `graphviz`)
- A genuine CMake project error (less common if CI is green on the same commit)

---

## 3. What to do

### 3.1 Confirm where it failed

1. Open the failed run and expand **Configure HealthGPS**.
2. Note whether the error is vcpkg, compiler, or a project CMake error.
3. Check whether **CI** on the same commit configured successfully with a `linux-*-release` preset and `-DBUILD_DOC=ON` (if applicable).

### 3.2 Align docs workflow with CI

In `.github/workflows/docs.yml`, keep vcpkg caching in sync with `.github/workflows/ci.yml`:

- `VCPKG_BINARY_SOURCES: clear;x-gha,readwrite`
- A step that exports `ACTIONS_CACHE_URL` and `ACTIONS_RUNTIME_TOKEN` before Install VCPKG / Configure

Commit, push to `main`, then re-run **Deploy Jekyll and Doxygen** (Actions → workflow → **Run workflow**).

### 3.3 If Configure still fails

1. Compare the Configure log with a successful CI configure on the same SHA.
2. Reproduce locally on Linux (or WSL):

   ```sh
   cmake --preset=linux-release -DBUILD_DOC=ON
   ninja -C out/build/linux-release/ doxygen-docs
   ```

3. Fix the underlying CMake/vcpkg issue; do not treat it as a Jekyll/markdown problem unless **Build with Jekyll** itself failed.

### 3.4 After a successful deploy

Smoke-check:

- [https://imperialchepi.github.io/healthgps/](https://imperialchepi.github.io/healthgps/)
- [https://imperialchepi.github.io/healthgps/api/](https://imperialchepi.github.io/healthgps/api/)

Remember nested paths under `user/`, `developer/`, and `technical/` may differ from older flat URLs.

---

## 4. Checklist

- [ ] Failed step is **Configure HealthGPS**, not Jekyll
- [ ] Read the Configure log (not only the annotation)
- [ ] `docs.yml` vcpkg cache matches `ci.yml` (`x-gha` + cache env export)
- [ ] Same commit’s CI configure is green (or understand why it isn’t)
- [ ] Re-run **Deploy Jekyll and Doxygen** after the fix
- [ ] Confirm `/` and `/api/` on Pages

---

**Author:** Mahima Ghosh
