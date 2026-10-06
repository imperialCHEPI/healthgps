# Global Health Policy Simulation model (Health-GPS)

[![CI](https://github.com/imperialCHEPI/healthgps/actions/workflows/ci.yml/badge.svg)](https://github.com/imperialCHEPI/healthgps/actions/workflows/ci.yml)
[![codecov](https://codecov.io/github/imperialCHEPI/healthgps/graph/badge.svg?token=745WKKE6X0)](https://codecov.io/github/imperialCHEPI/healthgps)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/imperialCHEPI/healthgps?include_prereleases)
![GitHub](https://img.shields.io/github/license/imperialCHEPI/healthgps)

| [Quick Start](#quick-start) | [Documentation](#documentation) | [February 2026 updates](#february-2026-updates) | [Development Tools](#development-tools) | [License](#license) | [Third-party Components](#third-party-components) |

Health-GPS microsimulation is part of the [STOP project](https://www.stopchildobesity.eu/), and supports researchers and policy makers in the analysis of the health and economic impacts of alternative measures to tackle *chronic diseases* and *obesity in children*. The model reproduces the characteristics of a population and simulates key individual event histories associated with key components of relevant behaviours, such as physical activity, and diseases such as diabetes or cancer.

Health-GPS has now been adapted to run for projects such as [FINCH](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/research/finch/), [JACARDI](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/research/jacardi/) and [JA PreventNCD](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/research/ja-prevent-ncd/). It can run for multiple projects using the inputs available at [HealthGPS-examples](https://github.com/imperialCHEPI/healthgps-examples) for each of the projects. Example: to run for STOP, use the [HLM_France](https://github.com/imperialCHEPI/healthgps-examples/tree/main/HLM_France) folder; for India the [KevinHall_India](https://github.com/imperialCHEPI/healthgps-examples/tree/main/KevinHall_India) folder; for FINCH the [KevinHall_FINCH](https://github.com/imperialCHEPI/healthgps-examples/tree/main/KevinHall_FINCH) folder.

The *Health GPS microsimulation* is being developed in collaboration between the [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/), Imperial College London; and [INRAE](https://www.inrae.fr), France; as part of the [STOP project](https://www.stopchildobesity.eu/). The software architecture uses a modular design approach to provide the building blocks of the *Health GPS application*, which is implemented using object-oriented principles in *Modern C++* programming language targeting the [C++20 standard](https://en.cppreference.com/w/cpp/20).

## Documentation

Full docs live under `[documentation/](documentation/README.md)`. Start there for indexes by audience.

| Need                                            | Document                                                                                                                                                |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Documentation home                              | [documentation/README.md](documentation/README.md)                                                                                                      |
| Site-style intro (diagrams)                     | [documentation/index.md](documentation/index.md)                                                                                                        |
| First run / binaries                            | [Quick Start](documentation/user/getstarted.md)                                                                                                         |
| Config, outputs, HPC                            | [User Guide](documentation/user/userguide.md)                                                                                                           |
| JSON schemas (diagrams)                         | [Configuration schemas](documentation/user/schemas.md)                                                                                                  |
| Models and module I/O                           | [Models overview](documentation/user/models-overview.md) · [Simulation models reference](documentation/technical/guides/simulation-models-reference.md) |
| Build from source / CMake                       | [Developer Guide](documentation/developer/development.md)                                                                                               |
| Architecture                                    | [Software Architecture](documentation/developer/architecture.md)                                                                                        |
| Data model / Datastore                          | [Data Model](documentation/developer/datamodel.md)                                                                                                      |
| Windows MSVC / Ninja (`cstdint`, `MSVCRTD.lib`) | [MSVC troubleshooting](documentation/developer/msvc-windows-build-troubleshooting.md)                                                                   |
| GitHub Pages deploy failed                      | [Docs deploy troubleshooting](documentation/developer/docs-deploy-troubleshooting.md)                                                                   |
| FINCH / income / predictors                     | [FINCH guide](documentation/technical/guides/finch-linear-models-and-income-adjustment.md)                                                              |
| Feb 2026 integrated changes                     | [Update report](documentation/technical/guides/healthgps-update-report-2026-02-20.md)                                                                   |
| Threading / HPC sizing                          | [Performance guide](documentation/technical/guides/performance-optimizations.md)                                                                        |
| Feature plans                                   | [technical/README.md](documentation/technical/README.md)                                                                                                |
| Doxygen API (GitHub Pages)                      | [API](https://imperialchepi.github.io/healthgps/api/)                                                                                                   |

Published website: [https://imperialchepi.github.io/healthgps/](https://imperialchepi.github.io/healthgps/). It is rebuilt from `documentation/` by the [docs workflow](.github/workflows/docs.yml) on **release** or **manual dispatch**, not on every push. Until that workflow runs against the current tree, the live site may lag the repo (older flat page layout).

## February 2026 updates

The **[HealthGPS Update Report – 20th Feb 2026](documentation/technical/guides/healthgps-update-report-2026-02-20.md)** summarises integrated changes (demographics, socioeconomic/income, static and dynamic risk factors, analysis/output, disease/PIF, policy, config/schema), parallelisation notes, and a developer file map. Snippets below are taken from that report.

**Supported use cases:** India, ADB, and FINCH on a shared codebase; backward compatibility with older India-style configs is retained alongside newer schema options.

**Module pipeline (simplified):**

```mermaid
flowchart LR
    DEMO[Demographic Module] --> SES[Socioeconomic Module]
    SES --> RF[Risk Factor Module]
    RF --> DIS[Disease Module]
    DIS --> IO[Read/write to files]
```

**Host application, run loop, module order, and output:**

```mermaid
%% Generated by https://gitdiagram.com/imperialchepi/healthgps
flowchart TD

subgraph group_host["Host and input"]
  node_cli["CLI host<br/>[program.cpp]"]
  node_configuration["Configuration<br/>[configuration.cpp]"]
  node_model_parser["Model parser<br/>[model_parser.cpp]"]
  node_data_manager["Data manager<br/>[datamanager.cpp]"]
  node_repository["Cached repository<br/>[repository.cpp]"]
  node_model_input["Model input<br/>[modelinput.h]"]
end

subgraph group_runtime["Simulation runtime"]
  node_runner["Trial runner<br/>[runner.cpp]"]
  node_simulation["Simulation engine<br/>[simulation.cpp]"]
  node_runtime_context["Runtime context"]
end

subgraph group_domains["Population model"]
  node_demographics["Demographics<br/>[demographic.cpp]"]
  node_ses["Socioeconomic module"]
  node_risk_factors["Risk factors<br/>[riskfactor.cpp]"]
  node_disease["Disease models<br/>[disease.cpp]"]
  node_scenarios["Policy scenarios"]
end

subgraph group_analysis["Analysis and output"]
  node_analysis_module["Analysis module"]
  node_event_bus["Event bus<br/>[event_bus.cpp]"]
  node_event_monitor["Event monitor<br/>[event_monitor.cpp]"]
  node_result_writer["Result writer"]
  node_tracking_writer["ID tracking"]
end

subgraph group_foundation["Shared foundations"]
  node_core_data["Core data types<br/>[forward_type.h]"]
  node_income_layout["Income layout"]
  node_string_util["String utilities<br/>[string_util.h]"]
end

node_researcher(("Researcher"))

node_researcher -->|"starts run"| node_cli
node_cli -->|"loads config"| node_configuration
node_cli -->|"loads data"| node_data_manager
node_configuration -->|"configures models"| node_model_parser
node_cli -->|"creates inputs"| node_model_input
node_data_manager -->|"supplies data"| node_repository
node_repository -.->|"provides definitions"| node_model_input
node_cli -->|"starts trials"| node_runner
node_runner -->|"runs simulations"| node_simulation
node_simulation -.->|"uses context"| node_runtime_context
node_simulation -->|"initializes population"| node_demographics
node_simulation -->|"updates population"| node_ses
node_simulation -->|"updates population"| node_risk_factors
node_simulation -->|"updates population"| node_disease
node_simulation -.->|"applies intervention"| node_scenarios
node_simulation -.->|"produces analysis"| node_analysis_module
node_cli -->|"creates bus"| node_event_bus
node_event_bus -->|"delivers events"| node_event_monitor
node_event_bus -->|"delivers results"| node_result_writer
node_event_bus -.->|"delivers tracking"| node_tracking_writer
node_model_input -.->|"uses types"| node_core_data
node_result_writer -->|"formats income output"| node_income_layout

click node_cli "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Console/program.cpp"
click node_configuration "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Input/configuration.cpp"
click node_model_parser "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Input/model_parser.cpp"
click node_data_manager "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Input/datamanager.cpp"
click node_repository "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/repository.cpp"
click node_model_input "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/modelinput.h"
click node_runner "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/runner.cpp"
click node_simulation "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/simulation.cpp"
click node_runtime_context "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/runtime_context.cpp"
click node_demographics "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/demographic.cpp"
click node_ses "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/ses_noise_module.cpp"
click node_risk_factors "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/riskfactor.cpp"
click node_disease "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/disease.cpp"
click node_scenarios "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/intervention_scenario.h"
click node_analysis_module "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/analysis_module.cpp"
click node_event_bus "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS/event_bus.cpp"
click node_event_monitor "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Console/event_monitor.cpp"
click node_result_writer "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Console/result_file_writer.cpp"
click node_tracking_writer "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Console/individual_id_tracking_writer.cpp"
click node_core_data "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Core/forward_type.h"
click node_income_layout "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Core/income_category_layout.h"
click node_string_util "https://github.com/imperialchepi/healthgps/blob/main/src/HealthGPS.Core/string_util.h"

classDef toneNeutral fill:#f8fafc,stroke:#334155,stroke-width:1.5px,color:#0f172a
classDef toneBlue fill:#dbeafe,stroke:#2563eb,stroke-width:1.5px,color:#172554
classDef toneAmber fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#78350f
classDef toneMint fill:#dcfce7,stroke:#16a34a,stroke-width:1.5px,color:#14532d
classDef toneRose fill:#ffe4e6,stroke:#e11d48,stroke-width:1.5px,color:#881337
classDef toneIndigo fill:#e0e7ff,stroke:#4f46e5,stroke-width:1.5px,color:#312e81
classDef toneTeal fill:#ccfbf1,stroke:#0f766e,stroke-width:1.5px,color:#134e4a
class node_cli,node_configuration,node_model_parser,node_data_manager,node_repository,node_model_input toneBlue
class node_runner,node_simulation,node_runtime_context toneAmber
class node_demographics,node_ses,node_risk_factors,node_disease,node_scenarios toneMint
class node_analysis_module,node_event_bus,node_event_monitor,node_result_writer,node_tracking_writer toneRose
class node_core_data,node_income_layout,node_string_util,node_researcher toneIndigo
```

**Person initialisation sequence (overview):**

```mermaid
flowchart TB
    A[Age] --> B[Gender]
    B --> C["Region (if available)"]
    C --> D["Ethnicity (if available)"]
    D --> E["Sector (if available)"]
    E --> F[Income]
    F --> G["Categorical: direct assignment (e.g. India)"]
    F --> H["Continuous: compute value, quartiles, assign categories (e.g. FINCH)"]
    G --> I[Risk Factors]
    H --> I
    I --> I1["Two-stage: Stage 1 logistic"]
    I1 --> I1b["Stage 2 Box-Cox"]
    I --> I2["Box-Cox only"]
    I1b --> RFA
    I2 --> RFA
    RFA["Risk factors assigned"] --> J[Physical Activity]
    J --> J1[Simple PA]
    J --> J2[Continuous PA]
    J1 --> PAD
    J2 --> PAD
    PAD["PA assigned"] --> K["Adjust to factors mean (if enabled)"]
    K --> L["Policies (if enabled)"]
    L --> M["Trends (if enabled)"]
    M --> N["UPF trends: multiplicative over time"]
    M --> O["Income trends: exponential decay"]
    N --> P[Trended Risk Factor Adjustment]
    O --> P
    P --> Q[Kevin Hall Model]
    Q --> R[Disease Model]
```

## Project Specific Requirements

Health-GPS is driven by config flags (not hard-coded project names). Optional `project_requirements` in `config.json` controls demographics (region, ethnicity, `gender2`), income type and final category count (`3` / `4` / `5`), physical activity, trends, and two-stage logistic behaviour. See:

- [Project requirements plan](documentation/technical/plans/project-requirements-plan.md)
- [User Guide: project requirements](documentation/user/userguide.md#project-requirements)
- Schema: `schemas/v1/config/project_requirements.json`

## Quick Start

The **Health GPS** application provides a command line interface (CLI) and runs on *Windows 10 (and newer)* and *Linux* devices. All supported options are provided to the model via a *configuration file* (JSON format), including intervention scenarios and multiple runs. Users are encouraged to start exploring the model by changing the provided example configuration file and running the model again.

Prefer `-c` / `--config` for the config path (file, folder, or zip URL). Put the backend datastore in `data.source` inside the config. Optional flags include `-T` / `--threads` (TBB cap) and `--dry-run`. Deprecated: `-f` / `--file` and `-s` / `--storage`.

From a Git Bash-style shell, run the console app with a config file and optional thread count:

```bash
/c/healthgps/out/build/windows-release/src/HealthGPS.Console/HealthGPS.Console.exe \
  -c /c/healthgps-examples/KevinHall_India/config.json \
  -T 2
```

First argument path: built executable HealthGPS.Console.exe.
-c: path to your JSON configuration (input / scenario).
-T: number of threads TBB may use for parallel work (example: 2).
If you omit -T, the model uses the maximum parallelism available on your machine (effectively up to the number of logical CPUs), subject to TBB defaults.

NOTE: If you specify the number of threads, a minimum of 2 threads is required.

Adjust the two paths to match where you built Health-GPS and where your `config.json` lives (e.g. PowerShell):

```bash
C:\healthgps\...\HealthGPS.Console.exe -c C:\healthgps-examples\...
```

For more information, see the [documentation home](documentation/README.md), [Quick Start](documentation/user/getstarted.md), and the [User Guide](documentation/user/userguide.md).

## Development Tools

The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++Standard Library. The project is fully managed by [CMake](https://cmake.org/) and [Microsoft Visual Studio](https://visualstudio.microsoft.com), the code base is portable but requires a C++20 compatible compiler to build. The development toolset uses [Ninja](https://ninja-build.org/) for build, [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automated builds.

For more information, see the [Developer Guide](documentation/developer/development.md). On Windows, if CMake cannot find headers such as `cstdint` or linking fails on `MSVCRTD.lib`, see [MSVC / Ninja troubleshooting](documentation/developer/msvc-windows-build-troubleshooting.md).

## License

The code in this repository is licensed under the [BSD 3-Clause](LICENSE.txt) license.

---

## Third-party components

### Libraries

| Name                                                          | License      |
| ------------------------------------------------------------- | ------------ |
| [Adevs](https://sourceforge.net/projects/adevs)               | BSD 3-Clause |
| [crossguid](https://github.com/graeme-hill/crossguid)         | MIT          |
| [cxxopts](https://github.com/jarro2783/cxxopts)               | MIT          |
| [eigen](https://eigen.tuxfamily.org)                          | MPL2         |
| [fmt](https://github.com/fmtlib/fmt)                          | MIT          |
| [nlohmann-json](https://github.com/nlohmann/json)             | MIT          |
| [jsoncons](https://github.com/danielaparker/jsoncons)         | Boost        |
| [rapidcsv](https://github.com/d99kris/rapidcsv)               | BSD 3-Clause |
| [oneAPI TBB](https://github.com/oneapi-src/oneTBB)            | Apache 2.0   |
| [libzippp](https://github.com/ctabin/libzippp)                | MIT          |
| [openssl](https://www.openssl.org)                            | Apache 2.0   |
| [PlatformFolders](https://github.com/sago007/PlatformFolders) | MIT          |
| [curlpp](http://www.curlpp.org)                               | MIT          |

### Tools and Frameworks

| Name                                               | License      |
| -------------------------------------------------- | ------------ |
| [vcpkg](https://github.com/microsoft/vcpkg)        | MIT          |
| [googletest](https://github.com/google/googletest) | BSD 3-Clause |

---

**Author:** Mahima Ghosh
