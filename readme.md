# Platypus Cradle Removal Algorithm

Platypus is a software solution that comes both as a standalone application and a Photoshop plugin. It is specifically designed to digitally remove cradling artifacts in X-ray images of paintings on panel. This project was made possible thanks to the financial support of the Kress foundation. Read more and download the software below.

## Cradle Removal

The Platypus cradle removal algorithm was originally developed by Ph.D. student Rachel Yin at the Department of Mathematics of the Duke University (North Carolina, USA). Rachel worked under the supervision of Professor Ingrid Daubechies. Later her code was translated to a C++ framework by Ph.D. student Gabor Fodor, from the department of Electronics and Informatics (ETRO) at the Vrije Universiteit Brussel (VUB), Belgium. Finally, the C++ code was embedded into a Photoshop plug-in, developed at Digital Film Tools. All three versions of the software, along with their documentation, are publicly available and can be downloaded below.


## Package Moved to Flatiron Institute

As of December 2024, the Flatiron Institute is taking over maintanence of the code for Platypus. The applet currently works on mac and linux. The photoshop plugin is in progress.

## Building from Source

### Prerequisites

You need the following installed before building:

- **git**
- **cmake** (3.16+)
- **ninja**
- A C++17 compiler (Xcode CLI tools on macOS, GCC on Linux, Visual Studio 2022 on Windows)

### Quick Start (Recommended)

The provided configure scripts prefer preinstalled OpenCV and Qt6 when available. If they are not found, the scripts fall back to downloading and building the dependencies via [vcpkg](https://vcpkg.io/).

#### macOS

```bash
git clone https://github.com/simonsfoundation/platypus.git
cd platypus
./configure.sh
cmake --build build --target PlatypusGui
./build/PlatypusGui
```

The script will install `ninja` via Homebrew if it is not already present.

#### Linux (Ubuntu/Debian)

Install the required system packages first (vcpkg needs X11/xcb headers to build Qt6's platform plugin):

```bash
sudo apt-get install -y pkg-config linux-libc-dev libx11-dev libxext-dev \
  libxrender-dev libxcb1-dev libx11-xcb-dev libxcb-glx0-dev \
  libxkbcommon-dev libxkbcommon-x11-dev libgl1-mesa-dev \
  curl zip unzip tar ninja-build cmake git
```

Then build:

```bash
git clone https://github.com/simonsfoundation/platypus.git
cd platypus
./configure.sh
cmake --build build --target PlatypusGui
./build/PlatypusGui
```

The configure script checks for missing system packages and tells you exactly what to install if anything is missing.

#### Windows

Requires [Git](https://git-scm.com/), [CMake](https://cmake.org/download/), and [Visual Studio 2022](https://visualstudio.microsoft.com/) (with C++ workload). Run from a **Developer Command Prompt**:

```bat
git clone https://github.com/simonsfoundation/platypus.git
cd platypus
configure.bat
cmake --build build --target PlatypusGui
build\PlatypusGui.exe
```

> **Note:** If `configure.sh` or `configure.bat` falls back to vcpkg, the first build takes ~30-60 minutes while OpenCV and Qt6 are built from source. Subsequent fallback builds are faster because vcpkg caches compiled packages locally.

### Manual Dependency Installation (Alternative)

If you prefer to install dependencies yourself rather than using the configure scripts:

#### macOS (Homebrew)

```bash
brew install opencv qt cmake ninja
cmake -B build -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --target PlatypusGui
```

#### Linux (apt)

```bash
sudo apt-get install -y libopencv-dev qt6-base-dev cmake ninja-build
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PlatypusGui
```

If `qt6-base-dev` is not available on your distribution, install the equivalent Qt6 development packages for your distro and point CMake at Qt6 with `-DCMAKE_PREFIX_PATH=/path/to/Qt6`.

#### Windows (vcpkg)

See the Windows GitHub Actions [workflow](https://github.com/simonsfoundation/platypus/blob/main/.github/workflows/build-and-release-windows.yml) for the full set of dependencies and build flags used in CI.
