#undef MOD_IDL
#define MOD_IDL "s.SurfaceControl:lListIndex,lArrayIndex,lEntrySize,lTotal,lArraySize\ns.SurfaceInfo:pData,lParentID,lBitmapID,lDisplayID,lFlags,lX,lY,lWidth,lHeight,lAbsX,lAbsY,wLevel,cBitsPerPixel,cBytesPerPixel,lLineWidth\ns.SurfaceList:pData,lParentID,lSurfaceID,lBitmapID,lDisplayID,lRootID,lPopOverID,lFlags,lX,lY,lWidth,lHeight,lLeft,lTop,lRight,lBottom,wLevel,wLineWidth,cBytesPerPixel,cBitsPerPixel,cCursor,ucOpacity\ns.SurfaceCoords:lX,lY,lWidth,lHeight,lAbsX,lAbsY\ns.PixelFormat:ucRedShift,ucGreenShift,ucBlueShift,ucAlphaShift,ucRedMask,ucGreenMask,ucBlueMask,ucAlphaMask,ucRedPos,ucGreenPos,ucBluePos,ucAlphaPos\ns.DisplayInfo:lDisplay,lFlags,wWidth,wHeight,wBitsPerPixel,wBytesPerPixel,xAccelFlags,lAmtColours,ePixelFormat:PixelFormat,fMinRefresh,fMaxRefresh,fRefreshRate,lIndex,lHDensity,lVDensity\ns.CursorInfo:lWidth,lHeight,lFlags,wBitsPerPixel\ns.BitmapSurface:pData,wWidth,wHeight,lLineWidth,ucBitsPerPixel,ucBytesPerPixel,ucOpacity,ucVersion,lColour,eClip:ClipRectangle,wXOffset,wYOffset,eFormat:ColourFormat,pPrivate\nc.ACF:SOFTWARE_BLIT=0x2,VIDEO_BLIT=0x1\nc.ARF:NO_DELAY=0x8,READ=0x1,UPDATE=0x4,WRITE=0x2\nc.BAF:BLEND=0x2,COPY=0x4,DITHER=0x1,FILL=0x1,LINEAR=0x8\nc.BDF:DITHER=0x4,REDRAW=0x2,SYNC=0x1\nc.BMF:ACCELERATED_2D=0x200,ACCELERATED_3D=0x400,ALPHA_CHANNEL=0x800,BLANK_PALETTE=0x1,CLEAR=0x80,COMPRESSED=0x2,FIXED_DEPTH=0x4000,INVERSE_ALPHA=0x20,MASK=0x10,NEVER_SHRINK=0x1000,NO_BLEND=0x8000,NO_DATA=0x4,PREMUL=0x10000,QUERIED=0x40,TRANSPARENT=0x8,USER=0x100,X11_DGA=0x2000\nc.BMP:CHUNKY=0x3,PLANAR=0x2\nc.CEF:DELETE=0x1,EXTEND=0x2\nc.CLF:DRAG_DROP=0x1,HOST=0x2\nc.CLIPTYPE:AUDIO=0x2,DATA=0x1,FILE=0x8,IMAGE=0x4,OBJECT=0x10,TEXT=0x20\nc.CRF:BUFFER=0x10,LMB=0x1,MMB=0x2,NO_BUTTONS=0x20,RESTRICT=0x8,RMB=0x4\nc.CS:CIE_LAB=0x3,CIE_LCH=0x4,LINEAR_RGB=0x2,SRGB=0x1\nc.CSRF:ALPHA=0x2,CLIP=0x10,DEFAULT_FORMAT=0x8,OFFSET=0x20,TRANSLUCENT=0x4,TRANSPARENT=0x1\nc.CT:AUDIO=0x1,DATA=0x0,END=0x6,FILE=0x3,IMAGE=0x2,OBJECT=0x4,TEXT=0x5\nc.DPMS:DEFAULT=0x0,OFF=0x1,STANDBY=0x3,SUSPEND=0x2\nc.DRAG:ANCHOR=0x1,NONE=0x0,NORMAL=0x2\nc.DSF:NO_DRAW=0x1,NO_EXPOSE=0x2\nc.DT:GLES=0x4,NATIVE=0x1,WINDOWS=0x3,X11=0x2\nc.EXF:ABSOLUTE=0x8,ABSOLUTE_COORDS=0x8,CHILDREN=0x1,CURSOR_SPLIT=0x10,REDRAW_VOLATILE=0x2,REDRAW_VOLATILE_OVERLAP=0x4\nc.FLIP:HORIZONTAL=0x1,VERTICAL=0x2\nc.GMF:SAVE=0x1\nc.HOST:STICK_TO_FRONT=0x3,TASKBAR=0x2,TRANSLUCENCE=0x4,TRANSPARENT=0x5,TRAY_ICON=0x1\nc.IRF:FORCE_DRAW=0x10,IGNORE_CHILDREN=0x2,IGNORE_NV_CHILDREN=0x1,RELATIVE=0x8,SINGLE_BITMAP=0x4\nc.LVF:EXPOSE_CHANGES=0x1\nc.PF:ANCHOR=0x4,UNUSED=0x1,VISIBLE=0x2\nc.RNF:AFTER_COPY=0x20000,ASPECT_RATIO=0x4000000,AUTO_QUIT=0x200,COMPOSITE=0x800000,CURSOR=0x8000,DISABLED=0x100,FAST_RESIZE=0x80,FIXED_BUFFER=0x40000,FIXED_DEPTH=0x200000,FULL_SCREEN=0x1000000,GRAB_FOCUS=0x20,HAS_FOCUS=0x40,HOST=0x400,IGNORE_FOCUS=0x2000000,INIT_ONLY=0x32c1d81,NO_FOCUS=0x100000,NO_HORIZONTAL=0x2000,NO_PRECOMPOSITE=0x800000,NO_VERTICAL=0x4000,PERVASIVE_COPY=0x80000,POINTER=0x8000,POST_COMPOSITE=0x800000,PRECOPY=0x800,READ_ONLY=0x28040,SCROLL_CONTENT=0x10000,STICKY=0x10,STICK_TO_BACK=0x2,STICK_TO_FRONT=0x4,TOTAL_REDRAW=0x400000,TRANSPARENT=0x1,VIDEO=0x1000,VISIBLE=0x8,VOLATILE=0x28800,WRITE_ONLY=0x1000\nc.RT:ROOT=0x1\nc.SCR:ALPHA_BLEND=0x40,AUTO_SAVE=0x2,BIT_6=0x10,BORDERLESS=0x20,BUFFER=0x4,COMPOSITE=0x40,CUSTOM_WINDOW=0x40000000,DPMS_ENABLED=0x8000000,FLIPPABLE=0x20000000,GTF_ENABLED=0x10000000,HOSTED=0x2000000,MAXIMISE=0x80000000,MAXSIZE=0x100000,NO_ACCELERATION=0x8,POWERSAVE=0x4000000,READ_ONLY=0xfe300019,REFRESH=0x200000,VISIBLE=0x1\nc.SMF:AUTO_DETECT=0x1,BIT_6=0x2\nc.SWIN:HOST=0x0,ICON_TRAY=0x2,NONE=0x3,TASKBAR=0x1\nc.WH:CLOSE=0x1\n"
