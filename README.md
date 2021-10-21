![windows](https://github.com/imperialCHEPI/healthgps/actions/workflows/msbuild.yml/badge.svg)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/imperialCHEPI/healthgps?include_prereleases)
![GitHub](https://img.shields.io/github/license/imperialCHEPI/healthgps)

# Global Health Policy Simulation model (Health-GPS)
| [Quick Start](#quick-start) | [License](#license) | [Third-party Components](#third-party-components) |

Health-GPS microsimulation is part of the [STOP project](https://www.stopchildobesity.eu/), and support researchers and policy makers in the analysis of the health and economic impacts of alternative measures to tackle *chronic diseases* and *obesity in children*. The model reproduces the characteristics of a population and simulates key individual event histories associated with key components of relevant behaviours, such as physical activity, and diseases such as diabetes or cancer.

Childhood obesity is one of the major public health challenges throughout the world and is rising at an alarming rate in most countries. In particular, the rates of increase in obesity prevalence in developing countries have been more than 30% higher than those in developed countries. Simulation models are especially useful for assessing the long-term impacts of the wide range of policies that will be needed to tackle the obesity epidemic.

The *Health GPS microsimulation* is being developed in collaboration between the [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/), Imperial College London; and [INRAE](https://www.inrae.fr), France; as part of the [STOP project](https://www.stopchildobesity.eu/). The software architecture uses a modular design approach to provide the building blocks of the `Health GPS application`, which is implemented using object-oriented principles in `Modern C++` programming language targeting the [C++20 standard](https://en.cppreference.com/w/cpp/20).

# Quick Start
The **Health GPS** application provides a command line interface (CLI) and runs on *Windows 10 (and newer)* devices. You may need to install the latest [Visual C++ Redistributable](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-160) on the machine, the application requires the `2019 x64` version to be installed.

1. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) binaries from the repository.
2. Unzip the file contents into a local directory of your choice (xxx).
3. Open a command terminal, e.g. [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), and navigate to the directory used in step 2 (xxx).
4. Run: `X:\xxx> .\HealthGPS.Console.exe -f ".\example\demo.json" -s ".\data"` where `-f` gives the *configuration file* fullname and
`-s` the path to the root folder of the *backend storage* respectivelly.
5. The default output folder is `C:\HealthGPS\Result`, but this can be changed in the *configuration file* `(demo.json)`.

All supported running options are provided to the model via a *configuration file* (JSON format), including intervention scenarios and multiple runs. Users are encouraged to start exploring the model by changing this configuration file and running the model again.

**NOTE:** *The development datasets provided in this example are limited to 2010-2030 time frame. It is provided for demonstration purpuse to showcase the model's usage, input and output data formats. The backend data storage can be populated with new datasets, the `index.json` file defines the storage structure and file names, it also stores metadata to identify the data sources and respective limits for validation.*

***Known Issue:*** Windows 10 support for VT (Virtual Terminal) / ANSI escape sequences is turned OFF by default, this is required to display colours on console / shell terminals. You can enable this feature manually by editing windows [registry keys](https://superuser.com/questions/413073/windows-console-with-ansi-colors-handling/1300251#1300251), however we recommend the use of [Windows Terminal](https://www.microsoft.com/en-gb/p/windows-terminal/9n0dx20hk701?rtc=1&activetab=pivot:overviewtab), which is a modern terminal application for command-line tools, has no such limitation, and is now distributed as part of the Windows 11 installation.

# Development Tools
The *Health GPS* software is written in modern, standard ANSI C++, targeting the [C++20 version](https://en.cppreference.com/w/cpp/20) and using the C++ Standard Library. The project is fully managed by [Microsoft Visual Studio](https://visualstudio.microsoft.com) 2019, therefore *Windows OS* only, however the code base is portable and should easily build on other platforms when [compilers](https://en.cppreference.com/w/cpp/compiler_support) supporting the C++20 standard become available. The development toolset users [vcpkg](https://github.com/microsoft/vcpkg) package manager for dependencies, [googletest](https://github.com/google/googletest) for unit testing and [GitHub Actions](https://docs.github.com/en/actions) for automate build.

To start working on the *Health GPS* code base, the development machine needs:
1. Windows 10 or newer
2. [Git](https://git-scm.com/downloads)
3. [Microsoft Visual Studio](https://visualstudio.microsoft.com) 2019 or newer.
4. The latest [vcpkg](https://github.com/microsoft/vcpkg) installed globally for Visual Studio projects.
5. Internet connection

Download the *Health GPS* source code to the local machine, we recommend somewhere like `C:\src` or `C:\source`, since otherwise you may run into path issues with the build systems.
```cmd
> git clone https://github.com/imperialCHEPI/healthgps
```
Finally, open the project solution in Visual Studio `...\healthgps\source\HelathGPS.sln` and hit build. The first build takes considerably longer than normal due to the initial work required by the package manager.

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



