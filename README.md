![windows](https://github.com/imperialCHEPI/healthgps/actions/workflows/windows.yml/badge.svg)
![Linux](https://github.com/imperialCHEPI/healthgps/actions/workflows/linux.yml/badge.svg)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/imperialCHEPI/healthgps?include_prereleases)
![GitHub](https://img.shields.io/github/license/imperialCHEPI/healthgps)

# Global Health Policy Simulation model (Health-GPS)
| [Quick Start](#quick-start) | [Development Tools](#development-tools) | [License](#license) | [Third-party Components](#third-party-components) |

Health-GPS microsimulation is part of the [STOP project](https://www.stopchildobesity.eu/), and support researchers and policy makers in the analysis of the health and economic impacts of alternative measures to tackle *chronic diseases* and *obesity in children*. The model reproduces the characteristics of a population and simulates key individual event histories associated with key components of relevant behaviours, such as physical activity, and diseases such as diabetes or cancer.

Childhood obesity is one of the major public health challenges throughout the world and is rising at an alarming rate in most countries. In particular, the rates of increase in obesity prevalence in developing countries have been more than 30% higher than those in developed countries. Simulation models are especially useful for assessing the long-term impacts of the wide range of policies that will be needed to tackle the obesity epidemic.

The *Health GPS microsimulation* is being developed in collaboration between the [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/), Imperial College London; and [INRAE](https://www.inrae.fr), France; as part of the [STOP project](https://www.stopchildobesity.eu/). The software architecture uses a modular design approach to provide the building blocks of the *Health GPS application*, which is implemented using object-oriented principles in *Modern C++* programming language targeting the [C++20 standard](https://en.cppreference.com/w/cpp/20).

# Quick Start

The **Health GPS** application provides a command line interface (CLI) and runs on *Windows 10 (and newer)* and *Linux* devices. All supported options are provided to the model via a *configuration file* (JSON format), including intervention scenarios and multiple runs. Users are encouraged to start exploring the model by changing the provided example configuration file and running the model again. See the [technical documentation](https://imperialchepi.github.io/healthgps/) for model description, user guides, software design, data model and development guidelines.

### Windows OS
You may need to install the latest [Visual C++ Redistributable](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-160) on the machine, the application requires the `2022 x64` version to be installed.

1. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) *source code* and *binaries* for Windows from the repository.
2. Unzip both files content into a local directory of your choice (xxx).
3. Open a command terminal, e.g. [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), and navigate to the directory used in step 2 (xxx).
4. Rename the *data source* subfolder (healthgps) by removing the version from folder's name.
5. Run: `X:\xxx> .\HealthGPS.Console.exe -f healthgps/example/France.Config.json -s healthgps/data` where `-f` gives the *configuration file* full-name and
`-s` the path to the root folder of the *backend storage* respectively.
6. The default output folder is `C:/healthgps/results/france`, but this can be changed in the *configuration file* `(France.Config.json)`.

### Linux OS
You may need to install the latest [GCC Compiler](https://gcc.gnu.org) and [Intel TBB](https://github.com/oneapi-src/oneTBB) runtime libraries in your system, the application requires `GCC 11.1` and `TBB 2018` or newer versions to be installed.

1. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) *source code* and *binaries* for Linux from the repository.
2. Unzip both files content into a local directory of your choice (xxx).
3. Open a command terminal and navigate to the directory used in step 2 (xxx).
4. Rename the *data source* subfolder (healthgps) by removing the version from folder's name.
4. Run: `user@machine:~/xxx$ ./HealthGPS.Console.exe -f healthgps/example/France.Config.json -s healthgps/data` where `-f` gives the *configuration file* full-name and
`-s` the path to the root folder of the *backend storage* respectively.
5. The default output folder is `$HOME/healthgps/results/france`, but this can be changed in the *configuration file* `(France.Config.json)`.

>**NOTE:** The development datasets provided in this example is for demonstration purpose, showcase the model's usage, input and output data formats. The backend data storage can be populated with new datasets, the `index.json` file defines the storage structure and file names, it also stores metadata to identify the data sources and respective limits for validation.*

>***Known Issue:*** Windows 10 support for VT (Virtual Terminal) / ANSI escape sequences is turned OFF by default, this is required to display colours on console / shell terminals. You can enable this feature manually by editing windows [registry keys](https://superuser.com/questions/413073/windows-console-with-ansi-colors-handling/1300251#1300251), however we recommend the use of [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), which is a modern terminal application for command-line tools, has no such limitation, and is now distributed as part of the Windows 11 installation.

# Development Tools
The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++ Standard Library. The project is fully managed by [CMake](https://cmake.org/) and [Microsoft Visual Studio](https://visualstudio.microsoft.com), the code base is portable but requires a C++20 compatible compiler to build. The development toolset users [Ninja](https://ninja-build.org/) for build, [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automate build.

To start working on the *Health GPS* code base, the suggested development machine needs:
1. Windows 10 or newer, [WSL 2](https://docs.microsoft.com/en-us/windows/wsl/) enabled and Linux distribution of choice installed.
2. [Git](https://git-scm.com/downloads) 2.3 or newer.
3. [CMake](https://cmake.org/) 3.20 or newer with support for [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html).
4. [Ninja](https://ninja-build.org/) 1.10 or newer.
5. [Microsoft Visual Studio](https://visualstudio.microsoft.com) 2022 or newer with CMake presets integration.
6. [GCC](https://gcc.gnu.org/) 11.1 or newer installed on *Linux* environment, plus.
   * [Intel TBB](https://github.com/oneapi-src/oneTBB) library version 2018 or later installed (e.g. `sudo apt update && sudo apt install libtbb-dev`) 
7. The latest [vcpkg](https://github.com/microsoft/vcpkg) installed globally for Visual Studio projects, and the VCPKG_ROOT environment variable set to the installation directory.
8. Internet connection.

Download the *Health GPS* source code to the local machine, we recommend somewhere like root `/src` or `/source`, since otherwise you may run into path issues with the build systems.
```cmd
> git clone https://github.com/imperialCHEPI/healthgps
```
Finally, open the `.../healthgps` folder in Visual Studio and hit build. The first build takes considerably longer than normal due to the initial work required by CMake and the package manager.

>**NOTE:** *This is the current toolset being used for developing the HealthGPS model, however CMake is supported by VS Code and many other IDE of choice, e.g. the model is current being compiled and built on Ubuntu Linux 22.04 LTS using only the CMake command line.*

### CMake Build

```cmd
cmake --list-presets=all

# Windows
cmake --preset='x64-release'
cmake --build --preset='release-build-windows' --target install --config Release

# Linux
cmake --preset='linux-release'
cmake --build --preset='release-build-linux' --target install --config Release
```
> To build the `Benchmark` application, add `-DBUILD_BENCHMARKS=ON` to the CMake configuration command (first for each OS).

The `HealthGPS` binaries will now be inside the `healthgps/out/install/[preset]/bin` directory.

To run the unit tests:
```cmd
# Windows
cmake --preset='x64-debug'
cmake --build --preset='debug-build-windows'
ctest --preset='core-test-windows'

# Linux
cmake --preset='linux-debug'
cmake --build --preset='debug-build-linux'
ctest --preset='core-test-linux'
```

All available options are defined using CMake *presets* in the `CMakePresets.json` file, which also declare *build presets* to be used via the CMake command line arguments.


# License

The code in this repository is licensed under the [BSD 3-Clause](LICENSE.md) license.

# Third-party components

## Libraries
| Name  | License |
|:---   |:---     |
| [Adevs](https://sourceforge.net/projects/adevs)                            | BSD 3-Clause |
| [crossguid](https://github.com/graeme-hill/crossguid)                      | MIT          |
| [cxxopts](https://github.com/jarro2783/cxxopts)                            | MIT          |
| [indicators](https://github.com/p-ranav/indicators)                        | MIT          |
| [fmt](https://github.com/fmtlib/fmt)                                       | MIT          |
| [nlohmann-json](https://github.com/nlohmann/json)                          | MIT          |
| [rapidcsv](https://github.com/d99kris/rapidcsv)                            | BSD 3-Clause |
| *Experimental* ||
| [arrow](https://github.com/apache/arrow)                                   | Apache-2.0   |
## Tools and Frameworks
| Name  | License |
|:---   |:---     |
| [vcpkg](https://github.com/microsoft/vcpkg)          | MIT          |
| [googletest](https://github.com/google/googletest)   | BSD 3-Clause |
| [benchmark](https://github.com/google/benchmark)     | Apache-2.0   |