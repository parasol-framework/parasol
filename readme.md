
# Parasol Framework

#### Web: https://parasol.ws

#### License: LGPL 2.1

## 1. Introduction

Parasol is an open source vector graphics engine and application framework for Windows and Linux.  It features integrated support for SVG with a focus on correctness, and we test against W3C's official SVG compliance tests.

Our integrated scripting language, Fluid, is based on LuaJIT and helps to simplify application development without compromising on speed or modern features.  Alternatively you can integrate the framework with your preferred language if it supports linking with standard system libraries and C function calls.

### Motivation

Parasol's ongoing development is focused on enhancing vector graphics programming on the desktop. We believe that this a research area that has been historically under-valued, and this needs to change with more displays achieving resolutions at 4K and beyond.  Besides from benefitting from the scalability of vector graphics and SVG features, we're also hoping to experiment with more dynamic rendering features that aren't possible with traditional bitmap interfaces.

### Features

* Multi-functional: Integrate your C++ code with our system libraries, or write programs quickly in Fluid, our integrated Lua-based scripting language.
* Build fully scalable UI's using our vector based widgets.  Windows, checkboxes, buttons, dialogs, text and more are supported.
* Load SVG files into a vector scene graph, interact with them via our API and save the output in SVG (saving is WIP).  Or just create vector scenes from scratch!
* Multi-platform compatible networking API, providing coverage for TCP/IP Sockets, HTTP, SSL.
* Integrated data handling APIs for XML, JSON, ZIP, PNG, JPEG, SVG.
* Full system abstraction for building cross-platform applications (file I/O, clipboards, threads, object management)
* Multi-channel audio playback supporting WAV and MP3 files.
* Hundreds of standardised scalable icons are included for application building.  Fonts are also standardised for cross-platform consistency.
* Can be used as an enhanced Lua framework by Lua developers in need of broad UI features and full system integration.
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

Source code should be checked out from the `release` branch of our GitHub repository if you are a newcomer:

```
git clone -b release https://github.com/parasol-framework/parasol.git parasol
```

Alternatively the `master` branch is generally stable and updated often, but be aware that minor build issues can occasionally surface.  Anything under `test` is under active development and unlikely to compile.

## 3. Build Process

We recommend using the GCC compiler to build the framework on all platforms.  If you are running Windows then we recommend using MSYS2 and MinGW as your build environment.  Please refer to section 2.3 of this document for Windows development instructions.  Targeting Android (experimental) will require Cygwin.

Linux systems require a few package dependencies to be installed first if a complete build is desired.  For an Apt based system such as Debian or Ubuntu, execute the following:

```
sudo apt-get install libasound2-dev libxrandr-dev libxxf86dga-dev cmake g++ xsltproc
```

To create the initial build you must run the following from the SDK's root folder with `<BUILD ENVIRONMENT>` set to the preferred build system on your platform, or if you don't know this then omit the option to get the default.  For Windows systems the correct build environment is `MinGW Makefiles`.

```
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release -G"<BUILD ENVIRONMENT>"
```

A full build and install can be performed with:

```
cmake --build release -j 8 -- -O
sudo cmake --install release
```

If problems occur at any stage during the build and you suspect an issue in the execution of a command, enable logging of the build process with the `--verbose` option.

### 3.1 Quick Builds

We recommend that you always build with the `-j 8 -- -O` set of options for best performance.  To limit the build to a specific sub-project you are working on, use `--target <NAME>`.  All known target names are printed during the output of a standard build.

### 3.2 Debug Build

Before resorting to a debug build, consider running the application with the `--log-api` option.  Doing so will print a wealth of information to stdout and this is often enough to resolve common problems quickly.

Parasol supports the use of `gdb` as a debugger.  Making a debug build for the first time will require a full build and install with the release options turned off, e.g:

```
cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug -G"<BUILD ENVIRONMENT>"
cmake --build debug -j 8 -- -O
sudo cmake --install debug
```

Now run `gdb` from the command-line and target parasol or the problem executable as required.  After fixing the issues, we strongly recommend returning to a standard build.  Debug builds have a very significant performance penalty and are not reflective of a 'real world environment' for day to day programming.

## 3.3 Windows Build Tools

MSYS2 and MinGW are required for a Windows build.  Once installed, the build process is essentially identical to that of Linux.  The install of the build tools can be completed as follows:

* Download the [MSYS2 archive](https://www.msys2.org/) and install to `C:\msys64`.
* Launch MSYS2 and run `pacman -Syu`; relaunch MSYS2 and run `pacman -Syu` again.
* Install all necessary dev packages with `pacman -S base-devel gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-toolchain`
* Install [Cmder](https://cmder.app/)
* Run Cmder, open Settings, then under Tasks add a new `MINGW64` shell with a command script of `set MSYSTEM=MINGW64 & set "PATH=/mingw64/bin;%PATH%" & C:\msys64\usr\bin\bash.exe --login -i`
* Open a new console tab and you should see a bash shell.  Enter `gcc --version` to ensure that the build environment's gcc executable is accessible.
* Follow the instructions from section 2 in this readme to perform a full compile, then proceed to section 3.

Please note that the default MSYS2 release of gcc is not supported.  The MinGW toolchain must be used as indicated above.

## 4. Running / Testing

After running `cmake --install <FOLDER>` the target installation folder will be printed to the console.  You may need to add this folder to your PATH variable permanently.

A successful install will allow you to run the `parasol` executable from the installation folder.  Run with `--help` to see the available options and confirm that the install worked correctly.  Example scripts are provided in the `examples` folder of this distribution.  We recommend starting with the widget example as follows:

```
parasol --log-error examples/widgets.fluid
```

Try running a second time with `--log-debug` to observe run-time log output while toying with the example.  Try a few of the other examples to get a feel for what you can achieve, and load them into a text editor to see how they were created.

## 5. Next Steps

Full documentation for developers is available online from our [main website](https://www.parasol.ws).

## 6. Source Code Licensing

Excluding third party APIs and marked contributions, the Parasol Framework is the copyright of Paul Manias © 1996 - 2023.  The source code is released under the terms of the LGPL as referenced below, except where otherwise indicated.

The Parasol Framework is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Font support is provided by source code originating from the FreeType Project, www.freetype.org
