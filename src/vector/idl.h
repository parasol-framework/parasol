#undef MOD_IDL
#define MOD_IDL "s.GradientStop:dOffset,eRGB:FRGB\ns.Transition:dOffset,sTransform\ns.VectorPoint:dX,dY,ucBit,ucBit\ns.VectorPainter:oPattern,oImage,oGradient,eColour:FRGB\ns.PathCommand:lType,ucLargeArc,ucSweep,ucPad1,dX,dY,dAbsX,dAbsY,dX2,dY2,dX3,dY3,dAngle\ns.VectorMatrix:pNext:VectorMatrix,oVector,dScaleX,dShearY,dShearX,dScaleY,dTranslateX,dTranslateY\ns.MergeSource:lSourceType,oEffect\nc.ARC:LARGE=0x1,SWEEP=0x2\nc.ARF:MEET=0x40,NONE=0x100,SLICE=0x80,X_MAX=0x4,X_MID=0x2,X_MIN=0x1,Y_MAX=0x20,Y_MID=0x10,Y_MIN=0x8\nc.CM:BRIGHTNESS=0x6,COLOURISE=0x9,CONTRAST=0x5,DESATURATE=0x8,HUE=0x7,HUE_ROTATE=0x3,LUMINANCE_ALPHA=0x4,MATRIX=0x1,NONE=0x0,SATURATE=0x2\nc.CMP:ALL=0xffffffff,ALPHA=0x3,BLUE=0x2,GREEN=0x1,RED=0x0\nc.EM:DUPLICATE=0x1,NONE=0x3,WRAP=0x2\nc.FM:CHILD_HAS_FOCUS=0x4,HAS_FOCUS=0x2,LOST_FOCUS=0x8,PATH_CHANGED=0x1\nc.LS:DISTANT=0x0,POINT=0x2,SPOT=0x1\nc.LT:DIFFUSE=0x0,SPECULAR=0x1\nc.MOP:DILATE=0x1,ERODE=0x0\nc.OP:ARITHMETIC=0x5,ATOP=0x3,BURN=0xe,CONTRAST=0xc,DARKEN=0x9,DIFFERENCE=0x11,DODGE=0xd,EXCLUSION=0x12,HARD_LIGHT=0xf,IN=0x1,INVERT=0xb,INVERT_RGB=0xa,LIGHTEN=0x8,MINUS=0x14,MULTIPLY=0x7,OUT=0x2,OVER=0x0,OVERLAY=0x15,PLUS=0x13,SCREEN=0x6,SOFT_LIGHT=0x10,SUBTRACT=0x14,XOR=0x4\nc.PE:Arc=0x11,ArcRel=0x12,ClosePath=0x13,Curve=0x9,CurveRel=0xa,HLine=0x5,HLineRel=0x6,Line=0x3,LineRel=0x4,Move=0x1,MoveRel=0x2,QuadCurve=0xd,QuadCurveRel=0xe,QuadSmooth=0xf,QuadSmoothRel=0x10,Smooth=0xb,SmoothRel=0xc,VLine=0x7,VLineRel=0x8\nc.RC:ALL=0x7,BASE_PATH=0x2,FINAL_PATH=0x1,TRANSFORM=0x4\nc.RQ:AUTO=0x0,BEST=0x4,CRISP=0x2,FAST=0x1,PRECISE=0x3\nc.TB:NOISE=0x1,TURBULENCE=0x0\nc.VBF:INCLUSIVE=0x1,NO_TRANSFORM=0x2\nc.VCS:INHERIT=0x0,LINEAR_RGB=0x2,SRGB=0x1\nc.VF:DISABLED=0x1,HAS_FOCUS=0x2\nc.VFR:END=0x4,EVEN_ODD=0x2,INHERIT=0x3,NON_ZERO=0x1\nc.VGF:FIXED_CX=0x2000,FIXED_CY=0x4000,FIXED_FX=0x8000,FIXED_FY=0x10000,FIXED_RADIUS=0x20000,FIXED_X1=0x200,FIXED_X2=0x800,FIXED_Y1=0x400,FIXED_Y2=0x1000,SCALED_CX=0x10,SCALED_CY=0x20,SCALED_FX=0x40,SCALED_FY=0x80,SCALED_RADIUS=0x100,SCALED_X1=0x1,SCALED_X2=0x4,SCALED_Y1=0x2,SCALED_Y2=0x8\nc.VGT:CONIC=0x2,CONTOUR=0x4,DIAMOND=0x3,LINEAR=0x0,RADIAL=0x1\nc.VIJ:BEVEL=0x1,INHERIT=0x5,JAG=0x3,MITER=0x2,ROUND=0x4\nc.VIS:COLLAPSE=0x2,HIDDEN=0x0,INHERIT=0x3,VISIBLE=0x1\nc.VLC:BUTT=0x1,INHERIT=0x4,ROUND=0x3,SQUARE=0x2\nc.VLJ:BEVEL=0x3,INHERIT=0x5,MITER=0x0,MITER_REVERT=0x1,MITER_ROUND=0x4,ROUND=0x2\nc.VMF:AUTO_SPACING=0x2,STRETCH=0x1,X_MAX=0x10,X_MID=0x8,X_MIN=0x4,Y_MAX=0x80,Y_MID=0x40,Y_MIN=0x20\nc.VOF:HIDDEN=0x1,INHERIT=0x3,SCROLL=0x2,VISIBLE=0x0\nc.VPF:BITMAP_SIZED=0x1,OUTLINE_VIEWPORTS=0x8,RENDER_TIME=0x2,RESIZE=0x4\nc.VSM:AUTO=0x0,BESSEL=0x8,BICUBIC=0x3,BILINEAR=0x2,BLACKMAN3=0xc,BLACKMAN8=0xf,GAUSSIAN=0x7,KAISER=0x5,LANCZOS3=0xb,LANCZOS8=0xe,MITCHELL=0x9,NEIGHBOUR=0x1,QUADRIC=0x6,SINC3=0xa,SINC8=0xd,SPLINE16=0x4\nc.VSPREAD:CLIP=0x6,END=0x7,PAD=0x1,REFLECT=0x2,REFLECT_X=0x4,REFLECT_Y=0x5,REPEAT=0x3,UNDEFINED=0x0\nc.VTS:CONDENSED=0x6,EXPANDED=0x8,EXTRA_CONDENSED=0x5,EXTRA_EXPANDED=0xb,INHERIT=0x0,NARROWER=0x3,NORMAL=0x1,SEMI_CONDENSED=0x7,SEMI_EXPANDED=0x9,ULTRA_CONDENSED=0x4,ULTRA_EXPANDED=0xa,WIDER=0x2\nc.VTXF:AREA_SELECTED=0x20,BLINK=0x8,EDIT=0x10,EDITABLE=0x10,LINE_THROUGH=0x4,NO_SYS_KEYS=0x40,OVERLINE=0x2,OVERWRITE=0x80,RASTER=0x200,SECRET=0x100,UNDERLINE=0x1\nc.VUNIT:BOUNDING_BOX=0x1,END=0x3,UNDEFINED=0x0,USERSPACE=0x2\nc.WVC:BOTTOM=0x3,NONE=0x1,TOP=0x2\nc.WVS:ANGLED=0x2,CURVED=0x1,SAWTOOTH=0x3\n"
