#undef MOD_IDL
#define MOD_IDL "s.PixelFormat:ucRedShift,ucGreenShift,ucBlueShift,ucAlphaShift,ucRedMask,ucGreenMask,ucBlueMask,ucAlphaMask,ucRedPos,ucGreenPos,ucBluePos,ucAlphaPos\ns.DisplayInfo:lDisplay,lFlags,wWidth,wHeight,wBitsPerPixel,wBytesPerPixel,xAccelFlags,lAmtColours,ePixelFormat:PixelFormat,fMinRefresh,fMaxRefresh,fRefreshRate,lIndex,lHDensity,lVDensity\ns.CursorInfo:lWidth,lHeight,lFlags,wBitsPerPixel\ns.BitmapSurface:pData,wWidth,wHeight,lLineWidth,ucBitsPerPixel,ucBytesPerPixel,ucOpacity,ucVersion,lColour,eClip:ClipRectangle,wXOffset,wYOffset,eFormat:ColourFormat,pPrivate\nc.SCR:COMPOSITE=0x40,MAXIMISE=0x80000000,VISIBLE=0x1,FLIPPABLE=0x20000000,GTF_ENABLED=0x10000000,DPMS_ENABLED=0x8000000,AUTO_SAVE=0x2,ALPHA_BLEND=0x40,HOSTED=0x2000000,NO_ACCELERATION=0x8,MAXSIZE=0x100000,BIT_6=0x10,REFRESH=0x200000,BORDERLESS=0x20,READ_ONLY=0xfe300019,CUSTOM_WINDOW=0x40000000,POWERSAVE=0x4000000,BUFFER=0x4\nc.GMF:SAVE=0x1\nc.PF:VISIBLE=0x2,UNUSED=0x1,ANCHOR=0x4\nc.BAF:COPY=0x4,BLEND=0x2,FILL=0x1,DITHER=0x1\nc.DPMS:OFF=0x1,SUSPEND=0x2,STANDBY=0x3,DEFAULT=0x0\nc.BMP:CHUNKY=0x3,PLANAR=0x2\nc.DT:NATIVE=0x1,WINDOWS=0x3,X11=0x2,GLES=0x4\nc.ACF:VIDEO_BLIT=0x1,SOFTWARE_BLIT=0x2\nc.SMF:BIT_6=0x2,AUTO_DETECT=0x1\nc.BMF:CLEAR=0x80,USER=0x100,ACCELERATED_2D=0x200,BLANK_PALETTE=0x1,X11_DGA=0x2000,COMPRESSED=0x2,ACCELERATED_3D=0x400,NO_DATA=0x4,ALPHA_CHANNEL=0x800,NEVER_SHRINK=0x1000,MASK=0x10,TRANSPARENT=0x8,INVERSE_ALPHA=0x20,FIXED_DEPTH=0x4000,QUERIED=0x40,NO_BLEND=0x8000\nc.FLIP:VERTICAL=0x2,HORIZONTAL=0x1\nc.CRF:RMB=0x4,RESTRICT=0x8,LMB=0x1,NO_BUTTONS=0x20,MMB=0x2,BUFFER=0x10\nc.HOST:TASKBAR=0x2,STICK_TO_FRONT=0x3,TRANSLUCENCE=0x4,TRANSPARENT=0x5,TRAY_ICON=0x1\nc.CSRF:TRANSLUCENT=0x4,DEFAULT_FORMAT=0x8,OFFSET=0x20,CLIP=0x10,TRANSPARENT=0x1,ALPHA=0x2\n"
