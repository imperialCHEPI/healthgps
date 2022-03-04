## Global Health Policy Simulation model

| [Home](index) | [Quick Start](getstarted) | [User Guide](userguide) | [Software Architecture](architecture) | [Data Model](datamodel) | Developer Guide |

# Developer Guide

The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++ Standard Library. The project is fully managed by [CMake](https://cmake.org/) and [Microsoft Visual Studio](https://visualstudio.microsoft.com), the code base is portable but requires a C++20 compatible compiler to build. The development toolset users [Ninja](https://ninja-build.org/) for build, [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automate build.

To start working on the *Health GPS* code base, the suggested development machine needs:
1. Windows 10 or newer, [WSL 2](https://docs.microsoft.com/en-us/windows/wsl/) enabled and Linux distribution of choice installed.
2. [Git](https://git-scm.com/downloads) 2.3 or newer.
3. [CMake](https://cmake.org/) 3.20 or newer with support for [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html).
4. [Ninja](https://ninja-build.org/) 1.10 or newer.
5. [Microsoft Visual Studio](https://visualstudio.microsoft.com) 2019 or newer with CMake presets integration.
6. [GCC](https://gcc.gnu.org/) 11.1 or newer installed on *Linux* environment.
7. The latest [vcpkg](https://github.com/microsoft/vcpkg) installed globally for Visual Studio projects, and the VCPKG_ROOT environment variable set to the installation directory.
8. Internet connection.

Download the *Health GPS* source code to the local machine, we recommend somewhere like `C:\src` or `C:\source`, since otherwise you may run into path issues with the build systems.
```cmd
> git clone https://github.com/imperialCHEPI/healthgps
```
Finally, open the *'.../healthgps/source'* folder in Visual Studio and hit build. The first build takes considerably longer than normal due to the initial work required by CMake and the package manager. Alternatively, there is also support for visual studio projects by opening the solution file `.../healthgps/source/HelathGPS.sln`, but this option will be removed in future version.

**NOTE:** *This is the current toolset being used for developing the HealthGPS model, however CMake is supported by VS Code and many other IDE of choice, e.g. the model is current being compiled and built on Ubuntu Linux 20.04 LTS using only the CMake command line.*

### CMake Build

```cmd
cmake --list-presets=all

# Windows
cmake --preset='x64-release'
cmake --build --preset='release-build-windows'

# Linux
cmake --preset='linux-release'
cmake --build --preset='release-build-linux'
```

The `HealthGPS` binary will now be inside the `./out/build/[preset]/HealthGPS.Console` directory.

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
All available options for CMake *prestes* are defined in the `CMakePresets.json` file, which also declare many of the options previously provided to CMake via command line arguments. The use of presets provides consistent builds scripts across development and CI/CD environments using source control for reproducibility.

## Third-party components

The project dependencies are included using [vcpkg](https://github.com/microsoft/vcpkg) package manager, and declared in the *vcpkg.json* manifest file. The package manager is integrated with CMake and automatically invoked by the ```find_package``` command during build.

| Name  | License |
|:---   |:---     |
| [Adevs](https://sourceforge.net/projects/adevs)                            | BSD 3-Clause |
| [crossguid](https://github.com/graeme-hill/crossguid)                      | MIT          |
| [cxxopts](https://github.com/jarro2783/cxxopts)                            | MIT          |
| [indicators](https://github.com/p-ranav/indicators)                        | MIT          |
| [fmt](https://github.com/fmtlib/fmt)                                       | MIT          |
| [nlohmann-json](https://github.com/nlohmann/json)                          | MIT          |
| [rapidcsv](https://github.com/d99kris/rapidcsv)                            | BSD 3-Clause |
| [googletest](https://github.com/google/googletest)   | BSD 3-Clause |

# Implementation

---
> **_UNDER DEVELOPMENT:_**  More content coming soon.
---

