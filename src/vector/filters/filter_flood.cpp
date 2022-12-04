/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
FloodFX: Applies the flood filter effect.

The FloodFX class is an output-only effect that fills its target area with a single colour value.

-END-

*********************************************************************************************************************/

class objFloodFX : public extFilterEffect {
   public:
   FRGB   Colour;
   RGB8   ColourRGB;
   DOUBLE Opacity;
};

//********************************************************************************************************************

static ERROR FLOODFX_NewObject(objFloodFX *Self, APTR Void)
{
   Self->Opacity = 1.0;
   Self->SourceType = VSF_NONE;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR FLOODFX_Draw(objFloodFX *Self, struct acDraw *Args)
{
   parasol::Log log;

   auto &filter = Self->Filter;

   // Draw to destination.  No anti-aliasing is applied and the alpha channel remains constant.
   // Note: There seems to be a quirk in the SVG standards in that flooding does not honour the
   // linear RGB space when blending.  This is indicated in the formal test results, but
   // W3C documentation has no mention of it.

   const auto col = agg::rgba8(Self->ColourRGB, F2T(Self->Colour.Alpha * Self->Opacity * 255.0));

   agg::rasterizer_scanline_aa<> raster;
   agg::renderer_base<agg::pixfmt_psl> renderBase;
   agg::scanline_p8 scanline;
   agg::pixfmt_psl format(*Self->Target);
   renderBase.attach(format);

   agg::path_storage path;
   path.move_to(filter->TargetX, filter->TargetY);
   path.line_to(filter->TargetX + filter->TargetWidth, filter->TargetY);
   path.line_to(filter->TargetX + filter->TargetWidth, filter->TargetY + filter->TargetHeight);
   path.line_to(filter->TargetX, filter->TargetY + filter->TargetHeight);
   path.close_polygon();

   agg::renderer_scanline_bin_solid< agg::renderer_base<agg::pixfmt_psl> > solid_render(renderBase);
   agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(path, filter->ClientVector->Transform);
   raster.add_path(final_path);
   renderBase.clip_box(Self->Target->Clip.Left, Self->Target->Clip.Top, Self->Target->Clip.Right - 1, Self->Target->Clip.Bottom - 1);
   solid_render.color(col);
   agg::render_scanlines(raster, scanline, solid_render);

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Colour: The colour of the fill in RGB float format.

This field defines the colour of the flood fill in floating-point RGBA format, in a range of 0 - 1.0 per component.

The colour is complemented by the #Opacity field.

*********************************************************************************************************************/

static ERROR FLOODFX_GET_Colour(objFloodFX *Self, FLOAT **Value, LONG *Elements)
{
   *Value = (FLOAT *)&Self->Colour;
   *Elements = 4;
   return ERR_Okay;
}

static ERROR FLOODFX_SET_Colour(objFloodFX *Self, FLOAT *Value, LONG Elements)
{
   parasol::Log log;
   if (Value) {
      if (Elements >= 3) {
         Self->Colour.Red   = Value[0];
         Self->Colour.Green = Value[1];
         Self->Colour.Blue  = Value[2];
         Self->Colour.Alpha = (Elements >= 4) ? Value[3] : 1.0;

         Self->ColourRGB.Red   = F2T(Self->Colour.Red * 255.0);
         Self->ColourRGB.Green = F2T(Self->Colour.Green * 255.0);
         Self->ColourRGB.Blue  = F2T(Self->Colour.Blue * 255.0);
         Self->ColourRGB.Alpha = F2T(Self->Colour.Alpha * 255.0);
      }
      else return log.warning(ERR_InvalidValue);
   }
   else Self->Colour.Alpha = 0;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Opacity: Modifies the opacity of the flood colour.

*********************************************************************************************************************/

static ERROR FLOODFX_GET_Opacity(objFloodFX *Self, DOUBLE *Value)
{
   *Value = Self->Opacity;
   return ERR_Okay;
}

static ERROR FLOODFX_SET_Opacity(objFloodFX *Self, DOUBLE Value)
{
   parasol::Log log;
   if ((Value >= 0.0) and (Value <= 1.0)) {
      Self->Opacity = Value;
      return ERR_Okay;
   }
   else return log.warning(ERR_OutOfRange);
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the effect.
-END-

*********************************************************************************************************************/

static ERROR FLOODFX_GET_XMLDef(objFloodFX *Self, STRING *Value)
{
   std::stringstream stream;

   stream << "<feFlood opacity=\"" << Self->Opacity << "\"/>";

   *Value = StrClone(stream.str().c_str());
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_flood_def.c"

static const FieldArray clFloodFXFields[] = {
   { "Colour",  FDF_VIRTUAL|FD_FLOAT|FDF_ARRAY|FD_RW,   0, (APTR)FLOODFX_GET_Colour, (APTR)FLOODFX_SET_Colour },
   { "Opacity", FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,          0, (APTR)FLOODFX_GET_Opacity, (APTR)FLOODFX_SET_Opacity },
   { "XMLDef",  FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)FLOODFX_GET_XMLDef, NULL },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_floodfx(void)
{
   return(CreateObject(ID_METACLASS, 0, &clFloodFX,
      FID_BaseClassID|TLONG, ID_FILTEREFFECT,
      FID_SubClassID|TLONG,  ID_FLOODFX,
      FID_Name|TSTRING,      "FloodFX",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,      clFloodFXActions,
      FID_Fields|TARRAY,     clFloodFXFields,
      FID_Size|TLONG,        sizeof(objFloodFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
