
# Parasol Framework

#### Web: http://www.parasol.ws

#### License: LGPL 2.1

## 1. Introduction

Parasol is a FOSS vector engine and application sandbox. It provides a fully integrated scripting language that is based on Lua to simplify application development without compromising on speed or modern features.

Parasol has a development bias towards vector graphics programming on the desktop, which we believe is an area that has been under-valued, particularly with recent increases in display resolution.  Our long term aim for Parasol is for it to be a credible open source leader in vector graphics programming and building multi-platform sandboxed applications.

In summary, Parasol provides:

* Vector graphics programming with an SVG compatible design.
* Networking (TCP/IP Sockets, HTTP, SSL).
* Widgets for UI development (windows, checkboxes, buttons, dialogs, text and a great deal more...).
* Data handling (XML, JSON, ZIP, PNG, JPEG, SVG)
* System abstraction for multi-platform support (file I/O, clipboards, threads, object management)
* Direct graphics access (h/w accelerated drawing)
* Multi-channel audio playback
* Text editing (implemented with scintilla.org)

Optional extensions not included with the main distribution:

* Cryptography support (AES)
* Database connectivity (MySQL, SQLite)

The Parasol environment is platform neutral and applications will work in the same way on all supported platforms.  In addition we are aiming for programs to be distributed in sand-boxed, permission-based archives so that users can feel comfortable running any software package without concerns of being compromised.

## 2. Build Process

We strongly recommend using the GCC compiler to build the framework.  If you are running Windows then we recommend using MSys and MinGW, which is a GCC port that is compatible with the makefiles in this archive.  Please refer to section 2.3 of this document for Windows development instructions.  Targeting Android will require Cygwin.

Linux systems require a few package dependencies to be installed first.  For an Apt based system such as Debian or Ubuntu, execute the following:

```
sudo apt-get install rsync libasound2-dev libxrandr-dev libxxf86dga-dev
```

To perform a full build from scratch, go to the root folder and execute:

```
make 3rdparty
make full-compile
```

To only build files that have changed, execute:

```
make compile
```

The build process will create a `release` folder for your platform and this will contain all of the necessary binaries and configuration files for a standard release of the Parasol Framework.  Do not use the release folder for running your Parasol build.  Doing so will result in changes to some release files, leading to a polluted build.  Refer to section 3 for information on how to run your build from an installation folder.

### 2.1 Quick Build

When working on modules in the core distribution, it is preferential to re-build that module only and not build a release from scratch.  To do this, `cd` to the module folder and enter `make`.  The resulting library or executable will be targeted to the release folder with no further action required on your part.

### 2.2 Debug Build

Before resorting to a debug build, consider running `parasol-cmd` with the `--log-info` option.  Doing so will print a wealth of information to stdout and this is often enough to resolve common problems quickly.

Parasol supports the use of `gdb` as a debugger.  Making a debug build for the first time will require a full recompile.  To enable debugging for your current session, execute the following:

```
export DEBUG=1
```

Then perform a rebuild from the parasol-build folder:

```
make full-compile
```

Now run `gdb` from the command-line and target parasol-cmd or the problem executable as required.  After fixing the issues, we strongly recommend returning to a standard build.  Debug builds are resource intensive and in particular are not reflective of a 'real world environment' for day to day programming.

To debug serious issues or to get more insight into a problem, set `VDEBUG=1` to enable verbose debugging, which greatly increases the quantity of messages being printed to the log.

## 2.3 Windows Build Tools

MSYS and MinGW are required for a Windows build.  Once installed, the build process is essentially identical to that of Linux.  The install of the build tools can be completed as follows:

* Download [MinGW-W64](http://sourceforge.net/projects/mingw-w64/)
* Install the exe with options `i686`, `win32`, `dwarf`.  Choose on install path of `C:\MinGW-W64`
* Download the [MSYS archive](http://sourceforge.net/projects/mingwbuilds/files/external-binary-packages)
* Unzip to `C:\`
* Edit `C:\msys\etc\fstab` and add `C:/MinGW-W64/mingw32   /mingw`
* Download a [YASM executable](http://yasm.tortall.net/Download.html) and move it to `C:\msys\bin\yasm.exe` (remove any version suffixes).
* Install [ConsoleZ](https://github.com/cbucher/console/releases)
* Run ConsoleZ, open Options, then change the ConsoleZ default shell to `C:\msys\bin\bash.exe --login -i`
* Open a new console tab and you should see a bash shell.  Enter `which gcc` to ensure that the build environment's gcc executable is accessible.  Double-check that the `fstab` folder reference and installation folder match up if gcc cannot be found.
* Follow the instructions from section 2 in this readme to perform a full compile, then proceed to section 3.

Installation notes:

* You may need to open makefile.inc in the SDK folder and change the `PARASOL_RELEASE` variable so that it refers to your core platform installation.  Please do not place any trailing slashes at the end of `PARASOL_RELEASE`.  Notice that in MSYS, the Windows drive letters are referenced as `/c/`, `/d/`, `/e/` and so forth, using forward slashes only.

## 3. Running / Testing

Run the following from the root folder to create a local installation of your release build:

```
make install
```

This will install the release to the 'install' folder.  If you would prefer to install to another location, open the makefile and change the `INSTALL` variable to your preferred path.

A successful install will allow you to run the `parasol` and `parasol-cmd` executable programs from the installation folder.  These programs are documented in full at our web-site if further details are required on their use.  To test that you have a working build, run these programs from the command-line with no additional parameters and observe the output for any errors.  You may wish to add the install folder to your `PATH` so that the executables are always accessible.

## 4. Next Steps

Full documentation for developers is available online from our main website.

### 4.1 Contributions

Parasol is an open source, open community project.  If you'd like to contribute to this project then please do so via the Git repository.  Create a feature branch for your work and then make a pull request targeting the develop branch in order to have the changes accepted.

The Parasol vector engine is dedicated to the memory of Maxim Shemanarev, whose seminal work on AGG made it possible.

## 5. Source Code Licensing

Excluding third party APIs and marked contributions, the Parasol Framework is the copyright of Paul Manias © 1996 - 2017.  The source code is released under the terms of the LGPL as referenced below, except where otherwise indicated.

The Parasol Framework is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Font support is provided by source code originating from the FreeType Project, www.freetype.org
