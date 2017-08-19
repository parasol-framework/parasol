#undef MOD_IDL
#define MOD_IDL "s.PixelFormat:ucRedShift,ucGreenShift,ucBlueShift,ucAlphaShift,ucRedMask,ucGreenMask,ucBlueMask,ucAlphaMask,ucRedPos,ucGreenPos,ucBluePos,ucAlphaPos\ns.DisplayInfo:lDisplay,lFlags,wWidth,wHeight,wBitsPerPixel,wBytesPerPixel,xAccelFlags,lAmtColours,ePixelFormat:PixelFormat,fMinRefresh,fMaxRefresh,fRefreshRate,lIndex,lHDensity,lVDensity\ns.CursorInfo:lWidth,lHeight,lFlags,wBitsPerPixel\ns.BitmapSurface:pData,wWidth,wHeight,lLineWidth,ucBitsPerPixel,ucBytesPerPixel,ucOpacity,ucVersion,lColour,eClip:ClipRectangle,wXOffset,wYOffset,eFormat:ColourFormat,pPrivate\nc.HOST:TASKBAR=0x2,TRANSPARENT=0x5,STICK_TO_FRONT=0x3,TRAY_ICON=0x1,TRANSLUCENCE=0x4\nc.PF:UNUSED=0x1,VISIBLE=0x2,ANCHOR=0x4\nc.DPMS:STANDBY=0x3,DEFAULT=0x0,OFF=0x1,SUSPEND=0x2\nc.CSRF:ALPHA=0x2,TRANSLUCENT=0x4,OFFSET=0x20,CLIP=0x10,TRANSPARENT=0x1,DEFAULT_FORMAT=0x8\nc.DT:WINDOWS=0x3,NATIVE=0x1,GLES=0x4,X11=0x2\nc.BMF:NO_BLEND=0x8000,FIXED_DEPTH=0x4000,X11_DGA=0x2000,QUERIED=0x40,ACCELERATED_3D=0x400,ALPHA_CHANNEL=0x800,TRANSPARENT=0x8,NO_DATA=0x4,ACCELERATED_2D=0x200,CLEAR=0x80,COMPRESSED=0x2,USER=0x100,NEVER_SHRINK=0x1000,BLANK_PALETTE=0x1,INVERSE_ALPHA=0x20,MASK=0x10\nc.BMP:CHUNKY=0x3,PLANAR=0x2\nc.GMF:SAVE=0x1\nc.SMF:AUTO_DETECT=0x1,BIT_6=0x2\nc.ACF:VIDEO_BLIT=0x1,SOFTWARE_BLIT=0x2\nc.SCR:CUSTOM_WINDOW=0x40000000,BIT_6=0x10,AUTO_SAVE=0x2,MAXSIZE=0x100000,NO_ACCELERATION=0x8,VISIBLE=0x1,COMPOSITE=0x40,READ_ONLY=0xfe300019,REFRESH=0x200000,DPMS_ENABLED=0x8000000,GTF_ENABLED=0x10000000,BORDERLESS=0x20,HOSTED=0x2000000,BUFFER=0x4,POWERSAVE=0x4000000,ALPHA_BLEND=0x40,MAXIMISE=0x80000000,FLIPPABLE=0x20000000\nc.BAF:COPY=0x4,BLEND=0x2,FILL=0x1,DITHER=0x1\nc.FLIP:VERTICAL=0x2,HORIZONTAL=0x1\nc.CSTF:BICUBIC=0x10,BRESENHAM=0x4,CLAMP=0x20,GOOD_QUALITY=0x1,CUBIC=0x10,NEIGHBOUR=0x8,FILTER_SOURCE=0x2,BILINEAR=0x1\nc.CRF:LMB=0x1,NO_BUTTONS=0x20,MMB=0x2,BUFFER=0x10,RMB=0x4,RESTRICT=0x8\n"
