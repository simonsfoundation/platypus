# Platypus Cradle Removal Algorithm

Platypus is a software solution that comes both as a standalone application and a Photoshop plugin. It is specifically designed to digitally remove cradling artifacts in X-ray images of paintings on panel. This project was made possible thanks to the financial support of the Kress foundation. Read more and download the software below.

## Cradle Removal

The Platypus cradle removal algorithm was originally developed by Ph.D. student Rachel Yin at the Department of Mathematics of the Duke University (North Carolina, USA). Rachel worked under the supervision of Professor Ingrid Daubechies. Later her code was translated to a C++ framework by Ph.D. student Gabor Fodor, from the department of Electronics and Informatics (ETRO) at the Vrije Universiteit Brussel (VUB), Belgium. Finally, the C++ code was embedded into a Photoshop plug-in, developed at Digital Film Tools. All three versions of the software, along with their documentation, are publicly available and can be downloaded below.


## Package Moved to Flatiron Institute

As of December 2024, the Flatiron Institute is taking over maintanence of the code for Platypus. The applet currently works on mac and linux. The photoshop plugin is in progress.

## Install From Source

The main dependencies for Platypus are

1. opencv
2. QT version 5
3. cmake
4. A modern c++ compiler

This is still a work in progress.

### Mac

```bash
brew install opencv qt@5 cmake
# make sure qt5 is on the path
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
# Run build
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release --target PlatypusGui
# Launch the App
./build/PlatypusGui
```

### Windows

TBD. See the windows github actions [here](https://github.com/simonsfoundation/platypus/blob/main/.github/workflows/build-and-release-windows.yml) for dependencies and build flags

### Linux

```bash
sudo apt-get update
sudo apt-get install -y libopencv-dev
# if this line does not work then you will need to download and install qt5 manually
# See here https://wiki.qt.io/Install_Qt_5_on_Ubuntu
sudo apt-get install -y qtbase5-dev qt5-qmake cmake
# Run build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4 --config Release --target PlatypusGui
# Launch the App
./build/PlatypusGui
```
