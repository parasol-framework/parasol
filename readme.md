
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
* Load SVG files into vector scene graphs, interact with them live via our API and changes will appear on the display automatically.
* SVG animation (SMIL) is supported.
* SVG support is tested against W3C's official SVG test suite, currently passing over 140 tests and counting.
* Use RIPL, a text layout engine modeled on HTML, SVG and word processing methodologies.
* Our multi-platform networking API provides coverage for TCP/IP sockets, HTTP and SSL.
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

Please refer to the following Wiki pages for information on how to build Parasol on our supported platforms:

* [Linux Builds](https://github.com/parasol-framework/parasol/wiki/Linux-Builds)
* [Windows Builds](https://github.com/parasol-framework/parasol/wiki/Windows-Builds)

A successful build and installation will create a `parasol` command tool, which is [documented here](https://github.com/parasol-framework/parasol/wiki/Parasol-Cmd-Tool).

Please refer to our [customisation wiki page](https://github.com/parasol-framework/parasol/wiki/Customising-Your-Build) for information on customising your build.

## 4. Next Steps

Full documentation for developers is available online from our [main website](https://www.parasol.ws).

## 5. Source Code Licensing

Excluding third party APIs and marked contributions, the Parasol Framework is the copyright of Paul Manias © 1996 - 2024.  The source code is released under the terms of the LGPL as referenced below, except where otherwise indicated.

The Parasol Framework is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Font support is provided by source code originating from the FreeType Project, www.freetype.org
