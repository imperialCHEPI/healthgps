# Global Health Policy Simulation model (Health-GPS)

[![CI](https://github.com/imperialCHEPI/healthgps/actions/workflows/ci.yml/badge.svg)](https://github.com/imperialCHEPI/healthgps/actions/workflows/ci.yml)
[![codecov](https://codecov.io/github/imperialCHEPI/healthgps/graph/badge.svg?token=745WKKE6X0)](https://codecov.io/github/imperialCHEPI/healthgps)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/imperialCHEPI/healthgps?include_prereleases)
![GitHub](https://img.shields.io/github/license/imperialCHEPI/healthgps)

| [Quick Start](#quick-start) | [Development Tools](#development-tools) | [License](#license) | [Third-party Components](#third-party-components) |

Health-GPS microsimulation is part of the [STOP project](https://www.stopchildobesity.eu/), and support researchers and policy makers in the analysis of the health and economic impacts of alternative measures to tackle *chronic diseases* and *obesity in children*. The model reproduces the characteristics of a population and simulates key individual event histories associated with key components of relevant behaviours, such as physical activity, and diseases such as diabetes or cancer.

Childhood obesity is one of the major public health challenges throughout the world and is rising at an alarming rate in most countries. In particular, the rates of increase in obesity prevalence in developing countries have been more than 30% higher than those in developed countries. Simulation models are especially useful for assessing the long-term impacts of the wide range of policies that will be needed to tackle the obesity epidemic.

The *Health GPS microsimulation* is being developed in collaboration between the [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/), Imperial College London; and [INRAE](https://www.inrae.fr), France; as part of the [STOP project](https://www.stopchildobesity.eu/). The software architecture uses a modular design approach to provide the building blocks of the *Health GPS application*, which is implemented using object-oriented principles in *Modern C++* programming language targeting the [C++20 standard](https://en.cppreference.com/w/cpp/20).

## Quick Start

The **Health GPS** application provides a command line interface (CLI) and runs on
*Windows 10 (and newer)* and *Linux* devices. All supported options are provided to the
model via a *configuration file* (JSON format), including intervention scenarios and
multiple runs. Users are encouraged to start exploring the model by changing the
provided example configuration file and running the model again.

For more information, see the [quick start guide] in the documentation.

[quick start guide]: https://imperialchepi.github.io/healthgps/getstarted

## Development Tools

The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++ Standard Library. The project is fully managed by [CMake](https://cmake.org/) and [Microsoft Visual Studio](https://visualstudio.microsoft.com), the code base is portable but requires a C++20 compatible compiler to build. The development toolset users [Ninja](https://ninja-build.org/) for build, [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automated builds.

For more information, see the [developer guide].

[developer guide]: https://imperialchepi.github.io/healthgps/development

## License

The code in this repository is licensed under the [BSD 3-Clause](LICENSE.txt) license.

## Third-party components

### Libraries

| Name                                                  | License      |
|:------------------------------------------------------|:-------------|
| [Adevs](https://sourceforge.net/projects/adevs)       | BSD 3-Clause |
| [crossguid](https://github.com/graeme-hill/crossguid) | MIT          |
| [cxxopts](https://github.com/jarro2783/cxxopts)       | MIT          |
| [eigen](https://eigen.tuxfamily.org)                  | MPL2         |
| [fmt](https://github.com/fmtlib/fmt)                  | MIT          |
| [nlohmann-json](https://github.com/nlohmann/json)     | MIT          |
| [rapidcsv](https://github.com/d99kris/rapidcsv)       | BSD 3-Clause |
| [oneAPI TBB](https://github.com/oneapi-src/oneTBB)    | Apache 2.0   |

### Tools and Frameworks

| Name                                               | License      |
|:---------------------------------------------------|:-------------|
| [vcpkg](https://github.com/microsoft/vcpkg)        | MIT          |
| [googletest](https://github.com/google/googletest) | BSD 3-Clause |
