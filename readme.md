
# Parasol Framework

#### Web: https://parasol.ws

#### License: LGPL 2.1

## 1. Introduction

Parasol is an open source vector graphics engine and application framework for Windows and Linux.  It features integrated support for SVG with a focus on correctness, and we test against W3C's official SVG compliance tests.

The API is written in C/C++ and is accessible as a standard library, allowing most popular programming languages to interface with it.  Our integrated scripting language, Fluid, is based on Lua and helps to simplify application development without compromising on speed or modern features.  Extensive API documentation is hosted at our website.

### Motivation

Parasol's ongoing development is focused on enhancing vector graphics programming on the desktop. We believe that this a research area that has been historically under-valued, and this needs to change with more displays achieving resolutions at 4K and beyond.  Besides from benefitting from the scalability of vector graphics and SVG features, we're also hoping to experiment with more dynamic rendering features that aren't possible with traditional bitmap interfaces.

### Features

* **Versatility:** Integrate your C++ code with our API, or write programs in Fluid, our integrated Lua-based scripting language.  Custom C++ builds are supported if you only need a particular feature such as the vector graphics engine for your project.
* **Scalable User Interfaces:** Create fully scalable UI's using our vector-based widgets, including windows, checkboxes, buttons, dialogs and text.  The script-driven architecture makes customization simple and flexible.
* **Live Vector Management:** Load SVG files into vector scene graphs and interact with the graph's properties in real-time via our API.
* **Animation:** Support for SVG animation (SMIL) is included, or use pre-canned visual effects from our VFX API.
* **W3C-Validated SVG Compliance:** We test against the W3C SVG test suite to maximise compatibility with the SVG standard.
* **Advanced Text Layout Engine:** Leverage RIPL, our powerful text layout engine inspired by HTML, SVG, and word-processing standards, for flexible document rendering that doesn't come excessive overheads.
* **Comprehensive Networking API:** Multi-platform networking support for TCP/IP sockets, HTTP, and SSL ensures secure and seamless connectivity.
* **Integrated Data Handling:** Built-in APIs for efficient handling of XML, JSON, ZIP, PNG, JPEG, and SVG file formats.
* **Cross-Platform System Abstraction:** Full system abstraction for platform-agnostic development, including file I/O, clipboard management, threading, and object management.
* **Multi-Channel Audio Playback:** Supports high-quality audio playback for WAV and MP3 files, enabling rich multimedia experiences.
* **Enhanced Lua Framework:** Ideal for Lua developers seeking robust UI features and comprehensive system integration. Parasol serves as an enhanced Lua framework for modern application development.
* **AI Ready:** The base repository includes everything a coding agent needs to understand and incorporate Parasol into your project, or just vibe your way to creating something new!

### Application Example

This is an example of a simple client application written in Fluid.  It loads an SVG file and displays the content in a window for the user.  Notice that the SVG is parsed in one line of code and all resource cleanup is handled in the background by the garbage collector.  You can find more example programs [here](examples/).

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

Release builds can be [downloaded directly](https://github.com/parasol-framework/parasol/releases/latest) from GitHub so that you don't need to compile the framework yourself.  If you're happy with using a release build then you can skip the rest of this readme and head to the [main website](https://www.parasol.ws) for further information on usage.

To build your own framework, checkout the source code from the `release` branch of our GitHub repository:

```
git clone -b release https://github.com/parasol-framework/parasol.git parasol
```

Alternatively the `master` branch is generally stable and updated often, but be aware that minor build issues can occasionally surface.  Anything under `test` is under active development and unlikely to compile.

## 3. Build Process

Please refer to the following Wiki pages for information on how to build Parasol on our supported platforms:

* [Linux Builds](https://github.com/parasol-framework/parasol/wiki/Linux-Builds)
* [Windows Builds](https://github.com/parasol-framework/parasol/wiki/Windows-Builds)

A successful build and installation will create a `parasol` command tool, which is [documented here](https://github.com/parasol-framework/parasol/wiki/Parasol-Cmd-Tool).

> [!TIP]
> Please refer to our [customisation wiki page](https://github.com/parasol-framework/parasol/wiki/Customising-Your-Build) for information on customising your build.

## 4. Next Steps

The [Wiki](https://github.com/parasol-framework/parasol/wiki) provides up-to-date documentation on most facets of the framework.

Detailed technical documentation for the Parasol APIs is available online at the [main website](https://www.parasol.ws).

## 5. Source Code Licensing

Excluding third party APIs and marked contributions, the Parasol Framework is the copyright of Paul Manias © 1996 - 2024.  The source code is released under the terms of the LGPL as referenced below, except where otherwise indicated.

The Parasol Framework is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Font support is provided by source code originating from the FreeType Project, www.freetype.org
