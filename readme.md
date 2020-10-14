
# Parasol Framework

#### Web: https://parasol.ws

#### License: LGPL 2.1

## 1. Introduction

Parasol is a FOSS vector engine and application sandbox. An integrated scripting language based on Lua helps to simplify application development without compromising on speed or modern features.  Alternatively you can integrate the framework with your preferred environment if it supports linking with standard system libraries.

Parasol's ongoing development is focused on enhancing vector graphics programming on the desktop. We believe that this a research area that has been historically under-valued, but this needs to change with more displays achieving resolutions at 4K and beyond.  Apart from the scalability of vector graphics, we're also hoping to experiment with more dynamic rendering features that aren't possible with traditional bitmap interfaces.

### Features

* Fully integrated Lua scripting in our Fluid development environment.
* Load and save SVG files.  Manipulate or create new vector scene graphs from scratch using our API that also includes feature enhancements not available in SVG.
* Multi-platform compatible networking API, providing coverage for TCP/IP Sockets, HTTP, SSL.
* Scalable widgets for UI development (windows, checkboxes, buttons, dialogs, text and a great deal more...).
* Data handling APIs (XML, JSON, ZIP, PNG, JPEG, SVG)
* Full system abstraction for multi-platform support (file I/O, clipboards, threads, object management)
* Multi-channel audio playback
* Extensive text editing widget (implemented with scintilla.org)

Optional extensions not included with the main distribution:

* Cryptography support (AES)
* Database connectivity (MySQL, SQLite)

## 2. Build Process

We recommend using the GCC compiler to build the framework on all platforms.  If you are running Windows then we recommend using MSYS2 and MinGW as your build environment.  Please refer to section 2.3 of this document for Windows development instructions.  Targeting Android will require Cygwin.

Linux systems require a few package dependencies to be installed first.  For an Apt based system such as Debian or Ubuntu, execute the following:

```
sudo apt-get install libasound2-dev libxrandr-dev libxxf86dga-dev
```

To create the initial build you must run the following from the SDK's root folder with `BUILD ENVIRONMENT` set to the preferred build system on your platform, or if you don't know then leave the option out to get the default.  For Windows systems the correct build environment is `MinGW Makefiles`.

```
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release -G"<BUILD ENVIRONMENT>"
```

A full build and install can be performed with:

```
cmake --build release -j 8 -- -O
sudo cmake --install release
```

If problems occur at any stage during the build and you suspect an issue in the execution of a command, enable logging of the build process with the `--verbose` option.

### 2.1 Quick Builds

We recommend that you always build with the `-j 8 -- -O` set of options for best performance.  To limit the build to a specific sub-project you are working on, use `--target <NAME>`.  All known target names are printed during the output of a standard build.

### 2.2 Debug Build

Before resorting to a debug build, consider running the application with the `--log-info` option.  Doing so will print a wealth of information to stdout and this is often enough to resolve common problems quickly.

Parasol supports the use of `gdb` as a debugger.  Making a debug build for the first time will require a full build and install with the release options turned off, e.g:

```
cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug -G"<BUILD ENVIRONMENT>"
cmake --build debug -j 8 -- -O
sudo cmake --install debug
```

Now run `gdb` from the command-line and target parasol or the problem executable as required.  After fixing the issues, we strongly recommend returning to a standard build.  Debug builds have a very significant performance penalty and are not reflective of a 'real world environment' for day to day programming.

## 2.3 Windows Build Tools

MSYS2 and MinGW are required for a Windows build.  Once installed, the build process is essentially identical to that of Linux.  The install of the build tools can be completed as follows:

* Download the [MSYS2 archive](https://www.msys2.org/) and install to `C:\msys64`.
* Launch MSYS2 and run `pacman -Syu`; relaunch MSYS2 and run `pacman -Su`
* Install dev packages with `pacman -S base-devel gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-toolchain`
* Install [Cmder](http://cmder.net/)
* Run Cmder, open Settings, then under Tasks add a new `MINGW64` shell with a command script of `set MSYSTEM=MINGW64 & set "PATH=/mingw64/bin;%PATH%" & C:\msys64\usr\bin\bash.exe --login -i`
* Open a new console tab and you should see a bash shell.  Enter `gcc --version` to ensure that the build environment's gcc executable is accessible.
* Follow the instructions from section 2 in this readme to perform a full compile, then proceed to section 3.

Please note that the default MSYS2 release of gcc is not supported.  The MinGW toolchain must be used as indicated above.

## 3. Running / Testing

After running `cmake --install <FOLDER>` the target installation folder will be printed to the console.  You may need to add this folder to your PATH variable permanently.

A successful install will allow you to run the `fluid` and `parasol` executable programs from the installation folder.  Run with `--help` to see the available options and confirm that the install worked correctly.  Example scripts are provided in the `examples` folder of this distribution.  We recommend starting with the widget example as follows:

```
fluid --log-error examples/widgets.fluid
```

Try running a second time with `--log-info` to observe run-time log output while toying with the example.  Try a few of the other examples to get a feel for what you can achieve, and load them into a text editor to see how they were created.

## 4. Next Steps

Full documentation for developers is available online from our [main website](https://www.parasol.ws).

## 5. Source Code Licensing

Excluding third party APIs and marked contributions, the Parasol Framework is the copyright of Paul Manias © 1996 - 2020.  The source code is released under the terms of the LGPL as referenced below, except where otherwise indicated.

The Parasol Framework is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Font support is provided by source code originating from the FreeType Project, www.freetype.org
