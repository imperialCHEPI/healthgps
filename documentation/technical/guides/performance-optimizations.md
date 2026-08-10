# Health-GPS performance and parallelization

## Global Health Policy Simulation model

| [Home](../../index.md) | [Quick Start](../../user/getstarted.md) | [User Guide](../../user/userguide.md) | [Schemas](../../user/schemas.md) | [Models](../../user/models-overview.md) | [Architecture](../../developer/architecture.md) | [Data Model](../../developer/datamodel.md) | [Developer Guide](../../developer/development.md) | [Technical docs](../README.md) | [API](https://imperialchepi.github.io/healthgps/api/) |

**Related:** [HealthGPS update report](healthgps-update-report-2026-02-20.md) (section 4) · [Developer Guide](../../developer/development.md) · [MSVC troubleshooting](../../developer/msvc-windows-build-troubleshooting.md) · [Technical index](../README.md)

This note describes **where Health-GPS uses parallelism today** and how to tune runtime on a laptop or HPC node. It replaces an older draft that described a non-existent `ParallelRunner` API.

---

## What is parallel today

Trials (replications) still run **one after another** inside `hgps::Runner` (`src/HealthGPS/runner.cpp`). There is **no** built-in “run four trials at once” mode in the Console.

Parallelism instead comes from:

| Area | Mechanism | Purpose |
| ---- | --------- | ------- |
| **Baseline + intervention** | Two `std::jthread` workers per trial when an intervention is configured | Both scenarios advance in the same replication |
| **In-process hot paths** | Intel [oneTBB](https://github.com/oneapi-src/oneTBB) and `core::parallel_for` / `core::run_async` | Population updates, disease incidence, analysis aggregation, income CSV writes |
| **Startup** | Async load of large datatables in `program.cpp` | Overlap I/O with setup |
| **Result I/O** | Separate dispatch threads in `EventMonitor` for main results vs individual ID tracking | Main JSON/CSV and `_IndividualIDTracking.csv` can be written concurrently |
| **Income-stratum CSVs** | `tbb::parallel_for_each` over income categories in `result_file_writer.cpp` | One file per stratum without serializing all writes on one lock |

For a module-by-module table and source links, see [Parallelization in the update report](healthgps-update-report-2026-02-20.md#4-parallelization).

---

## Thread limit (`--threads` / `-T`)

Health-GPS caps TBB worker threads from the Console CLI:

```bash
HealthGPS.Console -c path/to/config.json -T 64
```

- **`0`** (default): no explicit cap; TBB may use all visible CPU cores on the node.
- **Positive value**: sets `tbb::global_control::max_allowed_parallelism` in `src/HealthGPS.Console/program.cpp`.

On HPC, request **`ncpus`** in PBS (or your scheduler) to match what you pass to `-T`. Requesting 256 cores but limiting to 64 wastes queue priority; requesting 8 cores and omitting `-T` can oversubscribe the allocation.

See also the HPC thread note in the [Developer Guide](../../developer/development.md#hpc-build).

---

## HPC job sizing (practical)

There is no automatic “optimal core count.” Start from:

1. **Population size and trial count** in `config.json` (`running.trial_runs`, `inputs.settings.size_fraction`).
2. **One process per array task** when using PBS array jobs (parallelism across jobs, not inside one Console process).
3. **`-T`** set to the cores you reserved on that node (often 8–64 for France-scale examples).

Example job fragment (config holds `data.source`; prefer `-c` over deprecated `-f` / `-s`):

```bash
#PBS -l select=1:ncpus=8:mem=64gb
module add Health-GPS/X.Y.Z.B-GCCcore-11.3.0
HealthGPS.Console -c ${PBS_O_WORKDIR}/HLM_France/config.json -T 8 -j ${PBS_ARRAY_INDEX}
```

Use **array jobs** to scale replications across nodes; see [User Guide: HPC running](../../user/userguide.md#hpc-running).

---

## What stays sequential (on purpose)

- **Trial loop** in `Runner::run` (each replication finishes before the next starts).
- **Shared mutable state** protected by mutexes (analysis accumulators, disease counters, repository cache, event bus subscribers).
- **Random number streams** tied to run seeds for reproducibility.

Do not expect near-linear speedup by raising `-T` beyond the work available per simulated year; diminishing returns are normal once per-person loops are saturated.

---

## Further reading

| Topic | Document |
| ----- | -------- |
| Output threading design | [Parallelize output writes plan](../plans/parallelize-output-writes-plan.md) |
| Architecture / modules | [Software Architecture](../../developer/architecture.md) |
| FINCH / large configs | [FINCH guide](finch-linear-models-and-income-adjustment.md) |

---

**Author:** Mahima Ghosh
