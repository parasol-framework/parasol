
# Parasol Framework

#### Web: https://parasol.ws

#### License: LGPL 2.1

## 1. Introduction

Parasol is an open source vector graphics engine and application framework for Windows and Linux.  It features integrated support for SVG with a focus on correctness, and we test against W3C's official SVG compliance tests.

The API is written in C/C++ and is accessible as a standard library, allowing most popular programming languages to interface with it.  Our integrated scripting language, Fluid, is based on Lua and helps to simplify application development without compromising on speed or modern features.  Extensive API documentation is hosted at our website.

### Motivation

Parasol's ongoing development is focused on enhancing vector graphics programming on the desktop. We believe that this a research area that has been historically under-valued, and this needs to change with more displays achieving resolutions at 4K and beyond.  Besides from benefitting from the scalability of vector graphics and SVG features, we're also hoping to experiment with more dynamic rendering features that aren't possible with traditional bitmap interfaces.

### Features

* Multi-functional: Integrate your C++ code with our API, or write programs in Fluid, our integrated Lua-based scripting language.  Custom C++ builds are supported if you only need a particular feature such as the vector graphics engine for your project.
* Build fully scalable UI's using our vector based widgets.  Windows, checkboxes, buttons, dialogs, text and more are supported.  Our UI code is script driven, making customisation easy.
* Load SVG files into a vector scene graph, interact with them via our API and save the output in SVG (saving is WIP).  Or just create vector scenes from scratch!
* SVG animation (SMIL) is supported.
* Includes RIPL, a text layout engine modeled on HTML, SVG and word processing technologies.
* Multi-platform compatible networking API, providing coverage for TCP/IP Sockets, HTTP, SSL.
* Integrated data handling APIs for XML, JSON, ZIP, PNG, JPEG, SVG.
* Full system abstraction for building cross-platform applications (file I/O, clipboards, threads, object management)
* Multi-channel audio playback supporting WAV and MP3 files.
* Hundreds of standardised scalable icons are included for application building.  Fonts are also standardised for cross-platform consistency.
* Parasol can be used as an enhanced Lua framework by Lua developers in need of broad UI features and full system integration.
* WIP: Extensive text editing widget implemented with scintilla.org.

### Application Example

Here's an example of a simple client application written in Fluid.  It loads an SVG file and displays the content in a window for the user.  Notice that the SVG is parsed in one line of code and all resource cleanup is handled in the background by the garbage collector.  You can find more example programs [here](examples/).

```Lua
   require 'gui'
   require 'gui/window'

   if not arg('file') then
      print('Usage: --file [Path]')
      return
   end

   if mSys.AnalysePath(arg('file')) != ERR_Okay then
      error('Unable to load file ' .. arg('file'))
   end

   glWindow = gui.window({
      center = true,
      width  = arg('width', 800),
      height = arg('height', 600),
      title  = 'Picture Viewer',
      icon   = 'icons:programs/pictureviewer',
      minHeight = 200,
      minWidth  = 400
   })

   glViewport = glWindow.scene.new('VectorViewport', {
      aspectRatio='MEET', x=glWindow.client.left, y=glWindow.client.top,
      xOffset=glWindow.client.right, yOffset=glWindow.client.bottom
   })

   obj.new('svg', { target=glViewport, path=arg('file') })

   glWindow:setTitle(arg('file'))
   glWindow:show()

   processing.sleep()
```

## 2. Checkout

Release builds can be [downloaded directly](https://github.com/parasol-framework/parasol/releases/latest) from GitHub so that you don't need to compile the framework yourself.  If you're happy with downloading an archive then you can skip the rest of this readme and head to the [main website](https://www.parasol.ws) for further information on usage.

To build your own framework, checkout the source code from the `release` branch of our GitHub repository:

```
git clone -b release https://github.com/parasol-framework/parasol.git parasol
```

Alternatively the `master` branch is generally stable and updated often, but be aware that minor build issues can occasionally surface.  Anything under `test` is under active development and unlikely to compile.

## 3. Build Process

GCC is our recommended build tool for most platforms.  On Windows we use Visual Studio C++ to compile release builds, but you also have the option of using MSYS2 and MinGW as a GCC build environment.  Targeting Android (experimental) will require Cygwin.

### 3.1 Linux Builds (GCC)

Linux systems require a few package dependencies to be installed first if a complete build is desired.  For an Apt based system such as Debian or Ubuntu, execute the following:

```
sudo apt-get install libasound2-dev libxrandr-dev libxxf86dga-dev cmake g++ xsltproc
```

The following will configure the build process:

```
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release
```

A full build and install can be performed with:

```
cmake --build release -j 8 -- -O
sudo cmake --install release
```

If problems occur at any stage during the build and you suspect an issue in the execution of a command, enable logging of the build process with the `--verbose` option.

## 3.2 Windows Builds (GCC or Visual Studio)

On Windows you can choose between a Visual Studio (MSVC) build or a GCC build environment.  Between the two, we recommend using Visual Studio as it produces optimised builds approximately 25 to 33 percent smaller and 10% faster than the GCC equivalent.

### 3.2.1 Visual Studio Builds

If you opt to install the full [Visual Studio C++](https://visualstudio.microsoft.com/vs/features/cplusplus/) suite from Microsoft, it will do most of the heavy lifting for you if you open the parasol folder and it auto-detects Parasol's CMake files.  Consequently there is little instruction required if choosing that option, we just suggest adding `-j 8` to the cmake "Build command arguments" input box for faster builds.  It is possible to run Fluid scripts from the default build's `parasol.exe` program.  The `.vs/launch.vs.json` file manages the launch configuration of this program, and a working example is included in the project folder.

You can alternatively opt for a leaner build environment with MSVC Build Tools and get more hands-on with the build process.  Obtain the [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) and choose 'Desktop Development with C++' on install.  Your Start Menu will include a new launch option for 'Developer PowerShell for VS' that you can use to open a correctly preconfigured build environment.

Using PowerShell you should cd to the Parasol Framework folder and run cmake for configuration as follows:

```
cmake -S . -B visual-studio -DCMAKE_INSTALL_PREFIX=local -DBUILD_DEFS=OFF -DPARASOL_STATIC=ON
```

To compile a release build, run `cmake --build visual-studio -j 8 --config Release`.

To install a release build to the local folder, run `cmake --install visual-studio --config Release`.

Debug builds are created by switching from `--config Release` to `--config Debug` in the above.

### 3.2.2 GCC Builds for Windows

MSYS2 and MinGW are required for a GCC based Windows build.  Please note that the default MSYS2 release of GCC is not supported.  The MinGW toolchain must be installed as indicated below.

* Download the [MSYS2 archive](https://www.msys2.org/) and install to `C:\msys64`.
* Launch MSYS2 and run `pacman -Syu`; relaunch MSYS2 and run `pacman -Syu` again.
* Install all necessary dev packages with `pacman -S base-devel gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-toolchain`
* Install [Cmder](https://cmder.app/)
* Run Cmder, open Settings, then under Tasks add a new `MINGW64` shell with a command script of `set MSYSTEM=MINGW64 & set "PATH=/mingw64/bin;%PATH%" & C:\msys64\usr\bin\bash.exe --login -i`
* Open a new console tab and you should see a bash shell.  Enter `gcc --version` to ensure that the build environment's gcc executable is accessible.

From the Parasol Framework folder, the following will configure the build process, compile the framework and then install it:

```
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release -G"MinGW Makefiles"
cmake --build release -j 8 -- -O
cmake --install release
```

If you need to use GDB to debug the framework then the following would suffice.  In this case we are going to install to a local folder so that the build will not interfere with the release installation.

```
cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug -G"MinGW Makefiles" -DCMAKE_INSTALL_PREFIX=local -DBUILD_DEFS=OFF
cmake --build debug -j 8 -- -O
cmake --install debug
```

## 4. Running / Testing

After running `cmake --install <FOLDER>` the target installation folder will be printed to the console.  You may want to add this folder to your PATH variable permanently.

A successful install will allow you to run the `parasol` executable from the installation folder.  Run with `--help` to see the available options and confirm that the install worked correctly.  Example scripts are provided in the `examples` folder of this distribution.  We recommend starting with the widget example as follows:

```
parasol --log-error examples/widgets.fluid
```

Try running a second time with `--log-api` to observe run-time log output while toying with the example.  Try a few of the other examples to get a feel for what you can achieve, and load them into a text editor to see how they were created.

## 5. Build Options

The following build options and their default values may be of interest if you'd like to tweak the build process:

```
BUILD_TESTS       ON   Build tests (does not automatically run them).
BUILD_DEFS        ON   Auto-generate C/C++ headers and documentation.
RUN_ANYWHERE      OFF  Build a framework that can run from any folder without installation.
PARASOL_INSTALL   ON   Create installation targets.  If OFF, the build won't install anything.
INSTALL_EXAMPLES  OFF  Install the example scripts.
INSTALL_INCLUDES  OFF  Install the header files.
INSTALL_TESTS     OFF  Install the test programs.
ENABLE_ANALYSIS   OFF  Enable run-time address analysis if available.  Incompatible with gdb.
```

### 5.1 Static Builds

Parasol is built as a series of APIs such as 'core', 'display', 'network' and 'vector'.  Each API is compiled as an individual component.  A default system build compiles the APIs as shared libraries, as it prevents scripts and programs from loading unnecessary features.

If you're using Parasol for a specific run-time application that you're developing, you probably want a static build so that the framework is embedded with your application.  In addition, you can choose each specific API needed for your program - so if you didn't need networking, that entire category of features can be switched off for faster compilation and a smaller binary.

To enable a static build, use the `-DPARASOL_STATIC=ON` build option.  Your program's cmake file should link to the framework with `target_link_libraries (your_program PRIVATE ${INIT_LINK})`.

To choose the API's that you need, see the next section.

### 5.2 Disabling APIs

By default, every available API will be compiled in the framework unless they are individually switched off.  You can disable a given API with `-DDISABLE_<API_NAME>=TRUE`, where `<API_NAME>` is one of the following choices:

```
AUDIO      Audio API
DISPLAY    Display API
DOCUMENT   Document API   Dependent on Display, Vector, Font
FONT       Font API       Dependent on Display
HTTP       HTTP API       Dependent on Network
MP3        MP3 support    Dependent on Audio
NETWORK    Network API
PICTURE    Picture API    Dependent on Display
JPEG       JPEG support   Dependent on Picture
SCINTILLA  Scintilla API  Dependent on Display, Vector, Font
SVG        SVG support    Dependent on Display, Vector, Font
VECTOR     Vector API     Dependent on Display, Font
```

If you disable an API that has child dependencies, the dependent APIs will not be included in the build.  For instance, disabling Network will also result in HTTP being disabled.

## 6. Next Steps

Full documentation for developers is available online from our [main website](https://www.parasol.ws).

## 7. Source Code Licensing

Excluding third party APIs and marked contributions, the Parasol Framework is the copyright of Paul Manias © 1996 - 2024.  The source code is released under the terms of the LGPL as referenced below, except where otherwise indicated.

The Parasol Framework is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Font support is provided by source code originating from the FreeType Project, www.freetype.org
