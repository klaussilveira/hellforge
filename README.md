# HellForge
[![linux](https://github.com/klaussilveira/hellforge/actions/workflows/linux.yml/badge.svg?branch=master)](https://github.com/klaussilveira/hellforge/actions/workflows/linux.yml)
[![windows](https://github.com/klaussilveira/hellforge/actions/workflows/windows.yml/badge.svg?branch=master)](https://github.com/klaussilveira/hellforge/actions/workflows/windows.yml)
[![GitHub Release](https://img.shields.io/github/release/klaussilveira/hellforge.svg)](https://github.com/klaussilveira/hellforge/releases/latest)
[![License](https://img.shields.io/github/license/klaussilveira/hellforge)](https://github.com/klaussilveira/hellforge/blob/master/GPL)

HellForge is a fork of [DarkRadiant](https://www.darkradiant.net), a level editor for idTech4-based games. While DarkRadiant focuses primarily on The Dark Mod, HellForge is tailored for editing generic maps targeting [HellCore](https://github.com/klaussilveira/hellcore), [RBDOOM-3-BFG](https://github.com/RobertBeckebans/RBDOOM-3-BFG) and [dhewm3](https://github.com/dhewm/dhewm3) engines.

<p><img alt="HellForge" src="https://github.com/user-attachments/assets/b54ef820-bcf4-47a4-8eea-8cdbf6d3e213" /></p>

## Changes from DarkRadiant

HellForge has very opinionated features, keybindings and a different workflow. We contribute back to upstream DarkRadiant for features that are generic, like the array, scatter and polygon tools. However, other features would dramatically change how DarkRadiant operates and we don't want that.

- Create and manipulate brushes directly in the 3D camera view
- Visual preview mode for browsing textures
- `modelscale` spawnarg handling, replacing the old scaled model export workflow
- Quickly creates a sealing brush enclosing the selection bounding box
- Dark UI by default, with a theme manager system
- Control the opacity/transparency of utility textures (clip, caulk) in the viewport
- Settings panel to configure anti-aliasing and other visual options across all viewports

### New tools
- **Array Modifier**: Duplicates selected brushes/entities in configurable array patterns.
- **Scatter Tool**: Scatters copies of selected objects across surfaces with configurable parameters.
- **Polygon Tool**: Draw arbitrary polygon shapes in the 2D ortho views, allowing for a Doom 1-map editing workflow.
- **Decal Shooter**: Point-and-click tool for placing decal textures onto surfaces directly from the 3D camera view.
- **Terrain Generator**: Procedural terrain generation using Perlin and Simplex noise algorithms.
- **CSG Intersect**: Computes the intersection of selected brushes, keeping only the overlapping volume.
- **CSG Passable**: Turns solid volumes into wall shells and carves away interior geometry.
- **CSG Shell**: Similar to Passable but repositions brushes to avoid overlap.

## Getting started

HellForge requires game resources to work with, these resources are not installed by this editor. You'll need to point HellForge to one of the supported games (Doom 3, dhewm3, RBDOOM-3-BFG, etc.) before you can start working on your map.

# Compiling on Windows

## Prerequisites

HellForge is built on Windows using *Microsoft Visual Studio*, the free Community Edition can be obtained here:

*VC++ 2022:* https://visualstudio.microsoft.com/downloads/ (Choose Visual Studio Community)

When installing Studio please make sure to enable the "Desktop Development with C++" workload.

## Build

The main Visual C++ solution file is located in the root folder of this repository:

`DarkRadiant.sln`

Open this file with Visual Studio and start a build by right-clicking on the top-level
"Solution 'DarkRadiant'" item and choosing Build Solution. The executable will be placed in the `install/` folder.

### Windows Build Dependencies

Since HellForge requires a couple of open-source libraries that are not available on Windows by default, it will try to download and install the dependencies when the build starts. If it fails for some reason, you can try to run this script:

 `tools/scripts/download_windeps.ps1`

The dependencies packages need to be extracted into the main source directory, i.e. alongside the `include/` and `radiant/` directories.
Just drop the windeps.7z in the project folder and use 7-zip's "Extract to here"

# Compiling on Linux

## Prerequisites

To compile HellForge a number of libraries (with development headers) and a standards-compliant C++17 compiler are required. On an Ubuntu system the requirements may include any or all of the following packages:

* zlib1g-dev
* libjpeg-dev
* libwxgtk3.0-dev
* libxml2-dev
* libsigc++-2.0-dev
* libpng-dev
* libftgl-dev
* libglew-dev
* libalut-dev
* libvorbis-dev
* libgtest-dev
* libeigen3-dev
* libgit2-dev (optional)

To generate the local offline HTML user guide, the `asciidoctor` command must be in your
PATH. This is an optional dependency: if the command is not found, the CMake build will
proceed without building the user guide.

This does not include core development tools such as g++ or the git client to download the
sources (use sudo apt-get install git for that). One possible set of packages might be:

`sudo apt-get install git cmake g++ gettext pkg-config`

## Build

To build HellForge the standard CMake build process is used:

    $ cmake .
    $ make
    $ sudo make install

To install somewhere other than the default of `/usr/local`, use the `CMAKE_INSTALL_PREFIX` variable.

    $ cmake -DCMAKE_INSTALL_PREFIX=/opt/hellforge
    $ make
    $ sudo make install

Other useful variables are `CMAKE_BUILD_TYPE` to choose Debug or Release builds, `ENABLE_DM_PLUGINS` to disable the building of Dark Mod specific plugins (enabled by default), and `ENABLE_RELOCATION` to control whether HellForge uses hard-coded absolute paths like `/usr/lib` or paths relative to the binary (useful for certain package formats like Snappy or FlatPak).

# Compiling on macOS

## Prerequisites

You'll need an Xcode version supporting C++17 and the macOS 10.15 (Catalina) target at minimum. Xcode 11.3 should be working fine. You will need to install the Xcode command line tools to install MacPorts (run `xcode-select --install`)

To compile HellForge, a number of libraries (with development headers) are
required. You can obtain them by using [MacPorts](https://distfiles.macports.org/MacPorts/):

Install MacPorts, then open a fresh console and issue these commands:

    $ sudo port install jpeg wxwidgets-3.0 pkgconfig libsigcxx2 freetype ftgl glew
    $ sudo port install libxml2 freealut libvorbis libogg openal-soft eigen3

## Build

Start Xcode and open the project file in `tools/xcode/DarkRadiant.xcodeproj`.
Hit CMD-B to start the build, the output files will be placed to a folder
similar to this:

`~/Library/Developer/Xcode/DerivedData/DarkRadiant-somethingsomething/Build/Products/Release`

The application package in that folder can be launched right away or
copied to some location of your preference.

# License

The source code is published under the [GNU General Public License 2.0 (GPLv2)](http://www.gnu.org/licenses/gpl-2.0.html
), except for a few libraries which are using the BSD or MIT licenses, see the [LICENSE](https://raw.githubusercontent.com/klaussilveira/hellforge/master/LICENSE) file for specifics.
