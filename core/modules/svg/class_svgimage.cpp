/*****************************************************************************

-CLASS-
SVGImage: Renders SVG files to the UI.

The SVGImage class provides support for drawing vectors to the user's display.  Most of the underlying functionality
is provided by the @SVG and @Vector classes, with SVGImage only managing the rendering to a target
surface.

The following example illustrates how to create a simple vector scene using an embedded SVG definition:

<pre>
   local svgimage = obj.new("svgimage", { x=0, y=0, xOffset=0, yOffset=0 })

   svgimage.acDataFeed(0, DATA_XML, [[
&lt;svg viewBox="0 0 800 800" width="800" height="800"&gt;
  &lt;defs&gt;
    &lt;linearGradient id="LinearGradient"&gt;
      &lt;stop offset="5%" stop-color="#000000"/&gt;
      &lt;stop offset="95%" stop-color="#F0F060"/&gt;
    &lt;/&gt;

    &lt;radialGradient id="RadialGradient" cx="50%" cy="50%" r="80%"&gt;
      &lt;stop offset="30%" stop-color="#000000"/&gt;
      &lt;stop offset="60%" stop-color="#ffffff"/&gt;
    &lt;/&gt;
  &lt;/&gt;

  &lt;ellipse cx="50%" cy="50%" rx="5%" ry="5%" stroke-width="3" stroke="blue" fill="url(#LinearGradient)"/&gt;
&lt;/svg&gt;
]])
</pre>

Please refer to the W3C documentation on SVG for a complete reference to the attributes that can be applied to SVG
elements.  Unfortunately we do not support all SVG capabilities at this time, but support will improve in future.

Please refer to the @Layout class for information on how to set the coordinates for a SVG object.  In cases
where no coordinates or dimensions have been specified, the vector will take up the entire graphical area of its
related surface.

-END-

*****************************************************************************/

static void svgimage_animation(objSVG *Self)
{
   objSVGImage *context = (objSVGImage *)CurrentContext();
   acDrawID(context->Layout->SurfaceID);
}

//****************************************************************************

static void resize_vector(objSVGImage *Self)
{
   SetFields(Self->SVG->Scene,
      FID_PageWidth|TLONG,  Self->Layout->BoundWidth,
      FID_PageHeight|TLONG, Self->Layout->BoundHeight,
      TAGEND);
}

//****************************************************************************

static void draw_vector(objSVGImage *Self, objSurface *Surface, objBitmap *Bitmap)
{
   if (Self->Layout->Visible IS FALSE) return;
   if ((Self->SVG->Frame) AND (Surface->Frame != Self->SVG->Frame)) return;

   if ((Self->Layout->BoundWidth <= 0 ) OR (Self->Layout->BoundHeight <= 0)) return;

   Bitmap->XOffset += Self->Layout->BoundX;
   Bitmap->YOffset += Self->Layout->BoundY;

   SetFields(Self->SVG->Scene,
      FID_Bitmap|TPTR,      Bitmap,
      FID_PageWidth|TLONG,  Self->Layout->BoundWidth,
      FID_PageHeight|TLONG, Self->Layout->BoundHeight,
      TAGEND);

   acDraw(Self->SVG->Scene);

   Bitmap->XOffset -= Self->Layout->BoundX;
   Bitmap->YOffset -= Self->Layout->BoundY;
}

/*****************************************************************************
-ACTION-
Activate: Initiates playback of SVG animations.
-END-
*****************************************************************************/

static ERROR SVGIMAGE_Activate(objSVGImage *Self, APTR Void)
{
   SetFunctionPtr(Self->SVG, FID_FrameCallback, (APTR)&svgimage_animation);
   return acActivate(Self->SVG);
}

/*****************************************************************************
-ACTION-
Deactivate: Stops all playback of SVG animations.
-END-
*****************************************************************************/

static ERROR SVGIMAGE_Deactivate(objSVGImage *Self, APTR Void)
{
   return acDeactivate(Self->SVG);
}

/*****************************************************************************
-ACTION-
DataFeed: Vector graphics are created by passing XML-based instructions here.
-END-
*****************************************************************************/

static ERROR SVGIMAGE_DataFeed(objSVGImage *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_XML) {
      return load_svg(Self->SVG, 0, (CSTRING)Args->Buffer);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR SVGIMAGE_Free(objSVGImage *Self, APTR Void)
{
   if (Self->SVG)    { acFree(Self->SVG);    Self->SVG = NULL; }
   if (Self->Layout) { acFree(Self->Layout); Self->Layout = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR SVGIMAGE_Hide(objSVGImage *Self, APTR Void)
{
   return acHide(Self->Layout);
}

//****************************************************************************

static ERROR SVGIMAGE_Init(objSVGImage *Self, APTR Void)
{
   SetFunctionPtr(Self->Layout, FID_DrawCallback, (APTR)&draw_vector);
   SetFunctionPtr(Self->Layout, FID_ResizeCallback, (APTR)&resize_vector);
   if (!acInit(Self->Layout)) {
      if (!acInit(Self->SVG)) {
         SetFields(Self->SVG->Scene,
            FID_PageWidth|TLONG,  Self->Layout->BoundWidth,
            FID_PageHeight|TLONG, Self->Layout->BoundHeight,
            TAGEND);
         return acShow(Self->Layout);
      }
      else return ERR_Init;
   }
   else return ERR_Init;
}

//****************************************************************************

static ERROR SVGIMAGE_NewObject(objSVGImage *Self, APTR Void)
{
   if (!NewObject(ID_SVG, NF_INTEGRAL, &Self->SVG)) {
      if (!NewObject(ID_LAYOUT, NF_INTEGRAL, &Self->Layout)) {
         return ERR_Okay;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

//****************************************************************************

static ERROR SVGIMAGE_Show(objSVGImage *Self, APTR Void)
{
   return acShow(Self->Layout);
}

//****************************************************************************

#include "class_svgimage_def.c"

static const FieldArray clSVGImageFields[] = {
   { "SVG",    FDF_INTEGRAL|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "Layout", FDF_INTEGRAL|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   END_FIELD
};

static ERROR init_svgimage(void)
{
   return CreateObject(ID_METACLASS, 0, &clSVGImage,
      FID_ClassVersion|TFLOAT, VER_SVGIMAGE,
      FID_Name|TSTR,      "SVGImage",
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clSVGImageActions,
      FID_Fields|TARRAY,  clSVGImageFields,
      FID_Flags|TLONG,    CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Size|TLONG,     sizeof(objSVGImage),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}
