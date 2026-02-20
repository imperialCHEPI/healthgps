---
name: Parallelize output writes and is_active
overview: Parallelize result/tracking file writes by using two writer threads (main result vs individual tracking) and a second queue, then reduce redundant is_active() calls in hot paths (e.g. analysis module) by caching or filtering once per population iteration.
todos: []
isProject: false
---

# Parallelize output writes and reduce is_active() calls

## Current behaviour

- **Single result dispatch thread** ([event_monitor.cpp](src/HealthGPS.Console/event_monitor.cpp)): Both `EventType::result` and `EventType::individual_tracking` use `result_event_handler`, which pushes every message into one `results_queue`_. One thread (`result_dispatch_thread`) pops and calls `accept()` so that:
  - `ResultEventMessage` -> `result_writer_.write()` (JSON + main CSV + income CSVs)
  - `IndividualTrackingEventMessage` -> `individual_tracking_writer_->write()`
  All writes are therefore serialized on one thread.
- **Writers**:
  - [result_file_writer.cpp](src/HealthGPS.Console/result_file_writer.cpp): `ResultFileWriter::write()` holds a single mutex, then writes JSON fragment, main CSV rows, and (if enabled) income CSV rows. Different files (`stream_`, `csvstream_`, `income_csvstreams_`) but same lock.
  - [individual_id_tracking_writer.cpp](src/HealthGPS.Console/individual_id_tracking_writer.cpp): `IndividualIDTrackingWriter::write()` has its own mutex and writes only to the tracking CSV.
- **is_active()**: Called many times across the codebase (e.g. 13+ in [analysis_module.cpp](src/HealthGPS/analysis_module.cpp), plus demographic, disease, risk-factor modules). Each call is a simple member read (`is_alive_ && !has_emigrated_`), but in tight loops over the full population it can add up.

---

## Phase 1: Parallelize output writing

**Goal:** Let main result writes and individual-tracking writes run on different threads so they can proceed in parallel (different files, no shared state).

**Approach: two queues, two dispatch threads.**

1. **Add a second queue and thread in EventMonitor** ([event_monitor.h](src/HealthGPS.Console/event_monitor.h), [event_monitor.cpp](src/HealthGPS.Console/event_monitor.cpp)):

- Add `tbb::concurrent_queue<std::shared_ptr<hgps::EventMessage>> tracking_results_queue_` and a `tracking_dispatch_thread()` that loops popping from this queue and calling `m->accept(*this)` (same visitor; only `IndividualTrackingEventMessage` will be pushed here).
- Keep `results_queue_` and `result_dispatch_thread()` for `ResultEventMessage` only.

2. **Route messages by type**:

- In the **result** subscriber callback: keep pushing to `results_queue_` (unchanged).
- In the **individual_tracking** subscriber callback: push to `tracking_results_queue_` instead of `results_queue_`.

3. **Start and stop both threads**:

- In the ctor: besides `tg_.run([this] { result_dispatch_thread(); })`, add `tg_.run([this] { tracking_dispatch_thread(); })`.
- In `stop()` / dtor: `tg_context_.cancel_group_execution()` and `tg_.wait()` already wait for all tasks, so both threads will finish.

4. **Optional (later): parallelize inside ResultFileWriter::write()**

- Keep one mutex for the main writer (order of JSON + main CSV + income must stay consistent).
- Optionally build the JSON string and the main-CSV/income string chunks in parallel (e.g. TBB `parallel_invoke`) before taking the lock and writing; this reduces CPU before I/O but does not parallelize I/O itself. Can be a follow-up.

**Result:** Main result file(s) and individual-tracking file are written by two threads in parallel; ordering within each file is unchanged.

---

## Phase 2: Reduce number of is_active() calls

**Goal:** Avoid calling `is_active()` repeatedly on the same person in the same logical “iteration” (e.g. same year, same module).

**Where it’s used (examples):**

- [analysis_module.cpp](src/HealthGPS/analysis_module.cpp): many loops over `context.population()` that do `if (!entity.is_active()) continue;` or similar (e.g. lines 100, 224, 312, 418, 625, 685, 746, 1002, 1171, 1452, 1553, 1760, 1979).
- Other modules (demographic, default_disease_model, static_linear_model, kevin_hall_model, etc.) also iterate population and call `is_active()`.

**Options:**

1. **Cache per iteration in analysis module:** For a single “year” or “publish” pass, build once a compact structure (e.g. `std::vector<bool>` or bit set) of “active” by index, then in subsequent loops over the same population snapshot use that cache instead of calling `entity.is_active()`. Downside: population can change during a year (deaths, births); so the cache is only valid if all uses in that pass see the same snapshot. In `publish_result_message` and the various `calculate_`* paths, the population is not modified during the same call, so a cache per function scope is valid.
2. **Filter once into an “active” index list:** One loop over the population that builds `std::vector<std::size_t> active_indices`, then later loops iterate `for (auto i : active_indices) { auto& entity = population[i]; ... }`. This replaces many `is_active()` checks with one pass and then direct iteration. Same caveat: use within a single logical pass where population is not changing.
3. **Leave hot loops in disease/risk-factor modules as-is initially:** Focus on analysis_module where a single “result” computation does multiple full-population passes; there caching or a single “active” index list gives the biggest benefit.

**Recommendation:** Start with analysis_module only. In functions that do multiple passes over `context.population()` in one go (e.g. `calculate_historical_statistics`, or the block that does DALYs + risk-factor sums + comorbidity + prevalence), add one initial pass that fills `std::vector<bool> is_active(pop.size())` (or an index list), then use that in subsequent loops instead of calling `entity.is_active()`. Measure before/after if needed.

**Scope for plan:** Implement Phase 1 (two writer threads) first; then implement Phase 2 in analysis_module (one or two key functions) and leave a short comment for extending to other modules later.

---

## File change summary (Phase 1)

| File                                                         | Change                                                                                                                                                                                                       |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [event_monitor.h](src/HealthGPS.Console/event_monitor.h)     | Add `tracking_results_queue`_, declare `tracking_dispatch_thread()`, and second `tg_.run()` for it.                                                                                                          |
| [event_monitor.cpp](src/HealthGPS.Console/event_monitor.cpp) | individual_tracking subscriber pushes to `tracking_results_queue`_; implement `tracking_dispatch_thread()` (same loop as result_dispatch_thread but pop from tracking queue); start tracking thread in ctor. |

No changes to [result_file_writer.cpp](src/HealthGPS.Console/result_file_writer.cpp) or [individual_id_tracking_writer.cpp](src/HealthGPS.Console/individual_id_tracking_writer.cpp) for Phase 1; each writer stays single-threaded from its own dispatch thread.

---

## File change summary (Phase 2)

| File                                                     | Change                                                                                                                                                                                                                                                                  |
| -------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [analysis_module.cpp](src/HealthGPS/analysis_module.cpp) | In one or two hot functions that do multiple population passes (e.g. `calculate_historical_statistics`), add a single initial pass that builds a cache of active flags (or active indices), then use that cache in later loops instead of calling `person.is_active()`. |

---

## Order of implementation

1. Phase 1: EventMonitor two-queue, two-thread parallel writes.
2. Phase 2: In analysis_module, add is_active cache (or active index list) in the heaviest multi-pass function and replace repeated `is_active()` checks with the cache.
