---
name: Same person ID across baseline and intervention
overview: Assign Person IDs from population index (id = index + 1) so the same logical person has the same ID in both baseline and intervention runs. Changes are limited to Person and Population with MAHIMA comments; no change to scenario logic or event bus.
todos: []
isProject: false
---

# Same person ID across baseline and intervention

## Goal

Make the same logical person have the **same ID** in both baseline and intervention by deriving ID from **population index** (ID = index + 1) instead of a global counter. This enables tracking individuals across scenarios without breaking existing behaviour.

## Why this is safe

- **Scenario code** (physical_activity, marketing, food_labelling, fiscal): Each scenario holds its own `interventions_book`_ keyed by `entity.id()`. Baseline and intervention are separate Simulation/Scenario instances, so they have separate maps. Same ID in both runs does not cause collision.
- **No global ID key**: No code keys data globally by Person ID across runs; IDs are only used within one population.
- **result_file_writer.cpp** `message.id()` is `ResultEventMessage::id()` (event type enum), not Person ID — no change.

## Design: ID assignment rules


| Context                                      | ID assigned                           |
| -------------------------------------------- | ------------------------------------- |
| Initial population slot `i`                  | `i + 1`                               |
| Newborn replacing slot `i`                   | `i + 1` (slot keeps its ID)           |
| Newborn added via `emplace_back` (new slot)  | `people_.size()` (new index + 1)      |
| Person added via `add()` (immigration clone) | Set to slot index + 1 after placement |


```mermaid
flowchart LR
  subgraph baseline [Baseline Population]
    B0[Slot 0 ID 1]
    B1[Slot 1 ID 2]
    Bi[Slot i ID i+1]
  end
  subgraph intervention [Intervention Population]
    I0[Slot 0 ID 1]
    I1[Slot 1 ID 2]
    Ii[Slot i ID i+1]
  end
  B0 -.same logical person.-> I0
  B1 -.same logical person.-> I1
  Bi -.same logical person.-> Ii
```



## Implementation plan

### 1. Person ([person.h](src/HealthGPS/person.h), [person.cpp](src/HealthGPS/person.cpp))

- **Add constructors** (MAHIMA):
  - `Person(std::size_t id)` — sets `id_ = id` (for initial population construction).
  - `Person(core::Gender gender, std::size_t id)` — sets gender and `id_ = id` (for newborns).
- **Add** `void set_id(std::size_t id)` — allows Population to assign ID after placing a person (e.g. immigration clone). Document that it is for internal use by Population.
- **Keep** existing `Person()` and `Person(gender)` using `++Person::newUID` so standalone construction (tests, any code outside Population) remains unchanged and tests that expect distinct IDs for sequentially created persons still pass.
- **Comments**: Add a short MAHIMA block at the top of the ID-related section explaining index-based ID for same-person tracking across baseline and intervention.

### 2. Population ([population.h](src/HealthGPS/population.h), [population.cpp](src/HealthGPS/population.cpp))

- **Constructor** (MAHIMA): Replace `people_(size)` (default-constructed Persons) with a loop that creates each person with ID = index + 1:
  - e.g. `people_.reserve(size);` then `for (size_t i = 0; i < size; ++i) people_.emplace_back(i + 1);`
  - Use the new `Person(std::size_t id)` constructor.
- **add_newborn_babies** (MAHIMA):
  - When **replacing** a recycled slot at index `recycle.at(index)`: create `Person(gender, recycle.at(index) + 1)` so the newborn gets that slot’s ID.
  - When **emplace_back** (no recycle): create `Person(gender, people_.size() + 1)` so the new slot gets ID = new index + 1.
- **add** (MAHIMA):
  - After `people_.at(recycle.at(0)) = std::move(person)`: call `people_.at(recycle.at(0)).set_id(recycle.at(0) + 1)`.
  - After `people_.emplace_back(std::move(person))`: call `people_.back().set_id(people_.size())`.
- **Comments**: Add a MAHIMA block above the constructor and above add/add_newborn_babies explaining that IDs are index-based for cross-scenario tracking.

### 3. Tests ([Population.Test.cpp](src/HealthGPS.Tests/Population.Test.cpp))

- **CreateUniquePerson**: Currently expects `p2.id() > p1.id()` and `p1.id() == p3.id()`. `p3` is a reference to `p1`, so the second assertion is unchanged. The first relies on global uniqueness for two standalone `Person{}` — leave as-is (still using newUID).
- **CreateDefaultPerson**: Only checks `p.id() > 0` — still true for newUID.
- **AddSingleNewEntity / AddMultipleNewEntities**: They use `Population(init_size)` and then `add(Person{...})` or `add_newborn_babies(...)`. After our changes:
  - Initial population will have IDs 1..init_size.
  - Added persons will get IDs set by Population (recycled slot ID or new size). No change to test logic required; only IDs may differ from current (still positive and stable per slot).
- Add a **new test** (MAHIMA): e.g. "PersonIdEqualsSlotIndexPlusOne" — create a Population(10), assert `population[i].id() == i + 1` for several indices; add newborns (replace and emplace), assert the replaced slot keeps ID = slot_index + 1 and the new tail has ID = size.

### 4. No changes to

- **Simulation::partial_clone_entity**: Still returns `Person{}` (newUID). When that clone is passed to `population().add()`, Population will assign the correct slot-based ID via `set_id` — no change to this function.
- **DemographicModule**: Only assigns age, gender, region, ethnicity to existing `context.population()[index]`; does not create Person objects.
- **Scenario classes**: They only use `entity.id()` as key within their own map; same ID in baseline and intervention is desired and safe.
- **reset_id()**: Leave in place; can remain unused or used by tests. No need to call it for index-based ID.

### 5. Commenting convention (MAHIMA)

- At each changed site, add a short comment prefixed with `// MAHIMA:` explaining the change.
- In Person and Population, add a small block comment (e.g. before the new constructors and set_id, and above the Population constructor and add/add_newborn_babies) describing that IDs are index-based so that the same logical person has the same ID in baseline and intervention.

## File change summary


| File                                                           | Changes                                                                                                                                         |
| -------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| [person.h](src/HealthGPS/person.h)                             | Add `Person(std::size_t id)`, `Person(core::Gender, std::size_t id)`, `void set_id(std::size_t id)`; MAHIMA block for index-based ID.           |
| [person.cpp](src/HealthGPS/person.cpp)                         | Implement new constructors and `set_id`; MAHIMA comments.                                                                                       |
| [population.cpp](src/HealthGPS/population.cpp)                 | Constructor: build vector with `Person(i+1)`; add_newborn_babies: use ID = slot+1 or size+1; add: call set_id after placement; MAHIMA comments. |
| [Population.Test.cpp](src/HealthGPS.Tests/Population.Test.cpp) | Add test verifying ID == index + 1 for initial and after add/newborns; MAHIMA comment.                                                          |


## Order of implementation

1. Person: add constructors and `set_id`, with comments.
2. Population: constructor, then add_newborn_babies, then add, with comments.
3. Run existing tests; fix any that assume previous ID behaviour (only Population tests might need a new case).
4. Add the new Population test for index-based ID.

