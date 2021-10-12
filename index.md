## Welcome to the Global Health Policy Simulation model (Health-GPS).

| [Quick Start](#quick-start) | [Overview](#overview) | [User Guide](#user-guide) | [Developer Guide](#developer-guide) | [License](#license) | [Support](#support) |

Health-GPS microsimulation is part of the [STOP project](https://www.stopchildobesity.eu/), and support researchers and policy makers in the analysis of the health and economic impacts of alternative measures to tackle chronic diseases and obesity in children. The model reproduces the characteristics of a population and simulates key individual event histories associated with key components of relevant behaviours, such as physical activity, and diseases such as diabetes or cancer.

Childhood obesity is one of the major public health challenges throughout the world and is rising at an alarming rate in most countries. In particular, the rates of increase in obesity prevalence in developing countries have been more than 30% higher than those in developed countries. Simulation models are especially useful for assessing the long-term impacts of the wide range of policies that will be needed to tackle the obesity epidemic.

<a name="quick-start"></a>
### Quick Start
1. **Health GPS** command line interface (CLI) runs on *Windows 10 (and newer)* devices.
2. Download the latest [release](https://github.com/imperialCHEPI/healthgps/releases) binaries from the repository.
3. Unzip the file contents into a local directory of your choice (xxx).
4. Open a command terminal, e.g. PowerShell, and navigate to the directory used in step 2 (xxx).
5. Run: `X:\xxx> .\HealthGPS.Console.exe -f ".\example\demo.json" -s ".\data"` where `-f` gives the *configuration file* fullname and
`-s` the path to the root folder of the *backend storage* respectivelly.
5. The default output folder is `C:\HealthGPS\Result`, but this can be changed in the *configuration file* `(demo.json)`.

All supported running options are provided to the model via a *configuration file* (JSON format), including intervention scenarios and multiple runs. Users are encouraged to start exploring the model by changing this configuration file and running the model again.

**NOTE:** *The development datasets provided in this example are limited to 2010-2030 time frame. It is provided for demonstration purpuse to showcase the model's usage, input and output data formats. The backend data storage can be populated with new datasets, the `index.json` file defines the storage structure and file names.*

<a name="overview"></a>
### Overview

The *Health GPS microsimulation* is being developed by the [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/), Imperial College London; and [INRAE](https://www.inrae.fr), France; part of the [STOP project](https://www.stopchildobesity.eu/). The architecture uses a modular approach to software design to provide the building blocks of the Health GPS application, which is implemented using objected-oriented programming in Modern `C++20`.

<a name="user-guide"></a>
### User Guide
Coming soon.

<a name="developer-guide"></a>
### Developer Guide
Coming soon.

<a name="license"></a>
### License
The code in this repository is licensed under the [BSD 3-Clause](https://github.com/imperialCHEPI/healthgps/blob/main/LICENSE.md) license.

<a name="support"></a>
### Contact and Support
Imperial College London Business School, [Centre for Health Economics & Policy Innovation (CHEPI)](https://www.imperial.ac.uk/business-school/faculty-research/research-centres/centre-health-economics-policy-innovation/).
