/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
ImageFX: Renders a bitmap image in the effect pipeline.

The ImageFX class will render a source image into a given rectangle within the current user coordinate system.  The
client has the option of providing a pre-allocated Bitmap or the path to a @Picture file as the source.

If a pre-allocated @Bitmap is to be used, it must be created under the ownership of the ImageFX object, and this must
be done prior to initialisation.  It is required that the bitmap uses 32 bits per pixel and that the alpha channel is
enabled.

If a source picture file is referenced, it will be upscaled to meet the requirements automatically as needed.

Technically the ImageFX object is represented by a new viewport, the bounds of which are defined by attributes X, Y,
Width and Height.  The placement and scaling of the referenced image is controlled by the #AspectRatio field.

-END-

*********************************************************************************************************************/

typedef class rkImageFX : public objFilterEffect {
   public:
   objBitmap *Bitmap;    // Bitmap containing source image data.
   objPicture *Picture;  // Origin picture if loading a source file.
   LONG AspectRatio;     // Aspect ratio flags.
   LONG ResampleMethod;  // Resample method.
} objImageFX;

/*********************************************************************************************************************
-ACTION-
Draw: Render the effect to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR IMAGEFX_Draw(objImageFX *Self, struct acDraw *Args)
{
   parasol::Log log(__FUNCTION__);

   auto &filter = Self->Filter;

   std::array<DOUBLE, 4> bounds = { filter->ClientViewport->vpFixedWidth, filter->ClientViewport->vpFixedHeight, 0, 0 };
   calc_full_boundary((objVector *)filter->ClientVector, bounds, false, false);
   const DOUBLE b_x = trunc(bounds[0]);
   const DOUBLE b_y = trunc(bounds[1]);
   const DOUBLE b_width  = bounds[2] - bounds[0];
   const DOUBLE b_height = bounds[3] - bounds[1];

   DOUBLE target_x, target_y, target_width, target_height;
   if (filter->Units IS VUNIT_BOUNDING_BOX) {
      if (filter->Dimensions & DMF_FIXED_X) target_x = b_x;
      else if (filter->Dimensions & DMF_RELATIVE_X) target_x = trunc(b_x + (filter->X * b_width));
      else target_x = b_x;

      if (filter->Dimensions & DMF_FIXED_Y) target_y = b_y;
      else if (filter->Dimensions & DMF_RELATIVE_Y) target_y = trunc(b_y + (filter->Y * b_height));
      else target_y = b_y;

      if (filter->Dimensions & DMF_FIXED_WIDTH) target_width = filter->Width * b_width;
      else if (filter->Dimensions & DMF_RELATIVE_WIDTH) target_width = filter->Width * b_width;
      else target_width = b_width;

      if (filter->Dimensions & DMF_FIXED_HEIGHT) target_height = filter->Height * b_height;
      else if (filter->Dimensions & DMF_RELATIVE_HEIGHT) target_height = filter->Height * b_height;
      else target_height = b_height;
   }
   else { // USERSPACE
      if (filter->Dimensions & DMF_FIXED_X) target_x = trunc(filter->X);
      else if (filter->Dimensions & DMF_RELATIVE_X) target_x = trunc(filter->X * filter->ClientViewport->vpFixedWidth);
      else target_x = b_x;

      if (filter->Dimensions & DMF_FIXED_Y) target_y = trunc(filter->Y);
      else if (filter->Dimensions & DMF_RELATIVE_Y) target_y = trunc(filter->Y * filter->ClientViewport->vpFixedHeight);
      else target_y = b_y;

      if (filter->Dimensions & DMF_FIXED_WIDTH) target_width = filter->Width;
      else if (filter->Dimensions & DMF_RELATIVE_WIDTH) target_width = filter->Width * filter->ClientViewport->vpFixedWidth;
      else target_width = filter->ClientViewport->vpFixedWidth;

      if (filter->Dimensions & DMF_FIXED_HEIGHT) target_height = filter->Height;
      else if (filter->Dimensions & DMF_RELATIVE_HEIGHT) target_height = filter->Height * filter->ClientViewport->vpFixedHeight;
      else target_height = filter->ClientViewport->vpFixedHeight;
   }

   // The image's x,y,width,height default to (0,0,100%,100%) of the target region.

   DOUBLE img_x = target_x;
   DOUBLE img_y = target_y;
   DOUBLE img_width = target_width;
   DOUBLE img_height = target_height;

   if (filter->PrimitiveUnits IS VUNIT_BOUNDING_BOX) {
      // In this mode image dimensions typically remain at the default, i.e. (0,0,100%,100%) of the target.
      // If the user does set the XYWH of the image then 'fixed' coordinates act as multipliers, as if they were relative.

      // W3 spec on whether to use the bounds or the filter target region:
      // "Any length values within the filter definitions represent fractions or percentages of the bounding box
      // on the referencing element."

      const DOUBLE container_width = b_width;
      const DOUBLE container_height = b_height;
      if (Self->Dimensions & (DMF_FIXED_X|DMF_RELATIVE_X)) img_x = trunc(target_x + (Self->X * container_width));
      if (Self->Dimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y)) img_y = trunc(target_y + (Self->Y * container_height));
      if (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH)) img_width = Self->Width * container_width;
      if (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT)) img_height = Self->Height * container_height;
   }
   else {
      if (Self->Dimensions & DMF_RELATIVE_X)   img_x = target_x + (Self->X * target_width);
      else if (Self->Dimensions & DMF_FIXED_X) img_x = Self->X;

      if (Self->Dimensions & DMF_RELATIVE_Y)   img_y = target_y + (Self->Y * target_height);
      else if (Self->Dimensions & DMF_FIXED_Y) img_y = Self->Y;

      if (Self->Dimensions & DMF_RELATIVE_WIDTH)   img_width = target_width * Self->Width;
      else if (Self->Dimensions & DMF_FIXED_WIDTH) img_width = Self->Width;

      if (Self->Dimensions & DMF_RELATIVE_HEIGHT)   img_height = target_height * Self->Height;
      else if (Self->Dimensions & DMF_FIXED_HEIGHT) img_height = Self->Height;
   }

   DOUBLE xScale = 1, yScale = 1, align_x = 0, align_y = 0;
   calc_aspectratio("align_image", Self->AspectRatio, img_width, img_height, Self->Bitmap->Width, Self->Bitmap->Height, &align_x, &align_y, &xScale, &yScale);

   img_x += align_x;
   img_y += align_y;

   // Draw to destination

   agg::rasterizer_scanline_aa<> raster;
   agg::renderer_base<agg::pixfmt_psl> renderBase;
   agg::pixfmt_psl pixDest(*Self->Target);
   agg::pixfmt_psl pixSource(*Self->Bitmap);

   agg::path_storage path;
   path.move_to(target_x, target_y);
   path.line_to(target_x + target_width, target_y);
   path.line_to(target_x + target_width, target_y + target_height);
   path.line_to(target_x, target_y + target_height);
   path.close_polygon();

   renderBase.attach(pixDest);
   renderBase.clip_box(Self->Target->Clip.Left, Self->Target->Clip.Top, Self->Target->Clip.Right-1, Self->Target->Clip.Bottom-1);

   agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(path, filter->ClientVector->Transform);
   raster.add_path(final_path);

   agg::trans_affine img_transform;
   img_transform.scale(xScale, yScale);
   img_transform.translate(img_x, img_y);
   img_transform *= filter->ClientVector->Transform;
   img_transform.invert();
   agg::span_interpolator_linear<> interpolator(img_transform);

   agg::image_filter_lut ifilter;
   set_filter(ifilter, Self->ResampleMethod);

   agg::span_pattern_rkl<agg::pixfmt_psl> source(pixSource, 0, 0);
   agg::span_image_filter_rgba<agg::span_pattern_rkl<agg::pixfmt_psl>, agg::span_interpolator_linear<>> spangen(source, interpolator, ifilter);

   setRasterClip(raster, Self->Target->Clip.Left, Self->Target->Clip.Top,
      Self->Target->Clip.Right - Self->Target->Clip.Left,
      Self->Target->Clip.Bottom - Self->Target->Clip.Top);

   drawBitmapRender(renderBase, raster, spangen, 1.0);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR IMAGEFX_Free(objImageFX *Self, APTR Void)
{
   if (Self->Picture) { acFree(Self->Picture); Self->Picture = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR IMAGEFX_Init(objImageFX *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->Bitmap) return log.warning(ERR_UndefinedField);

   if ((Self->Filter->ColourSpace IS VCS_LINEAR_RGB) and (Self->Bitmap)) {
      bmpConvertToLinear(Self->Bitmap);
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// If the client attaches a bitmap as a child of our object, we use it as the primary image source.

static ERROR IMAGEFX_NewChild(objImageFX *Self, struct acNewChild *Args)
{
   parasol::Log log;

   if (Args->Object->ClassID IS ID_BITMAP) {
      if (!Self->Bitmap) {
         if (Self->Bitmap->BytesPerPixel IS 4) Self->Bitmap = (objBitmap *)Args->Object;
         else log.warning("Attached bitmap ignored; BPP of %d != 4", Self->Bitmap->BytesPerPixel);
      }
      else log.warning("Attached bitmap ignored; Bitmap field already defined.");
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR IMAGEFX_NewObject(objImageFX *Self, APTR Void)
{
   Self->AspectRatio    = ARF_X_MID|ARF_Y_MID|ARF_MEET;
   Self->ResampleMethod = VSM_BILINEAR;
   Self->SourceType     = VSF_PREVIOUS;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
AspectRatio: SVG compliant aspect ratio settings.
Lookup: ARF

*********************************************************************************************************************/

static ERROR IMAGEFX_GET_AspectRatio(objImageFX *Self, LONG *Value)
{
   *Value = Self->AspectRatio;
   return ERR_Okay;
}

static ERROR IMAGEFX_SET_AspectRatio(objImageFX *Self, LONG Value)
{
   Self->AspectRatio = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Bitmap: The @Bitmap being used as the image source.

Reading the Bitmap field will return the @Bitmap that is being used as the image source.  Note that if a custom
Bitmap is to be used, the correct way to do this as to assign it to the ImageFX object via ownership rules.

If a picture image has been processed by setting the #Path, the Bitmap will refer to the content that has been
processed.

*********************************************************************************************************************/

static ERROR IMAGEFX_GET_Bitmap(objImageFX *Self, objBitmap **Value)
{
   *Value = Self->Bitmap;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Path: Path to an image file supported by the Picture class.

*********************************************************************************************************************/

static ERROR IMAGEFX_GET_Path(objImageFX *Self, STRING *Value)
{
   if (Self->Picture) return GetString(Self->Picture, FID_Path, Value);
   else *Value = NULL;
   return ERR_Okay;
}

static ERROR IMAGEFX_SET_Path(objImageFX *Self, CSTRING Value)
{
   if ((Self->Bitmap) or (Self->Picture)) return ERR_Failed;

   if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &Self->Picture,
         FID_Path|TSTR,          Value,
         FID_BitsPerPixel|TLONG, 32,
         FID_Flags|TLONG,        PCF_FORCE_ALPHA_32,
         TAGEND)) {
      Self->Bitmap = Self->Picture->Bitmap;
      return ERR_Okay;
   }
   else return ERR_CreateObject;
}

/*********************************************************************************************************************

-FIELD-
ResampleMethod: The resample algorithm to use for transforming the source image.

*********************************************************************************************************************/

static ERROR IMAGEFX_GET_ResampleMethod(objImageFX *Self, LONG *Value)
{
   *Value = Self->ResampleMethod;
   return ERR_Okay;
}

static ERROR IMAGEFX_SET_ResampleMethod(objImageFX *Self, LONG Value)
{
   Self->ResampleMethod = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERROR IMAGEFX_GET_XMLDef(objImageFX *Self, STRING *Value)
{
   *Value = StrClone("feImage");
   return ERR_Okay;
}

//********************************************************************************************************************

static const FieldDef clResampleMethod[] = {
   { "Auto",      VSM_AUTO },
   { "Neighbour", VSM_NEIGHBOUR },
   { "Bilinear",  VSM_BILINEAR },
   { "Bicubic",   VSM_BICUBIC },
   { "Spline16",  VSM_SPLINE16 },
   { "Kaiser",    VSM_KAISER },
   { "Quadric",   VSM_QUADRIC },
   { "Gaussian",  VSM_GAUSSIAN },
   { "Bessel",    VSM_BESSEL },
   { "Mitchell",  VSM_MITCHELL },
   { "Sinc3",     VSM_SINC3 },
   { "Lanczos3",  VSM_LANCZOS3 },
   { "Blackman3", VSM_BLACKMAN3 },
   { "Sinc8",     VSM_SINC8 },
   { "Lanczos8",  VSM_LANCZOS8 },
   { "Blackman8", VSM_BLACKMAN8 },
   { NULL, 0 }
};

#include "filter_image_def.c"

static const FieldArray clImageFXFields[] = {
   { "Bitmap",         FDF_VIRTUAL|FDF_OBJECT|FDF_R,           ID_BITMAP, (APTR)IMAGEFX_GET_Bitmap, NULL },
   { "Path",           FDF_VIRTUAL|FDF_STRING|FDF_RI,          0, (APTR)IMAGEFX_GET_Path, (APTR)IMAGEFX_SET_Path },
   { "XMLDef",         FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)IMAGEFX_GET_XMLDef, NULL },
   { "AspectRatio",    FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clAspectRatio, (APTR)IMAGEFX_GET_AspectRatio, (APTR)IMAGEFX_SET_AspectRatio },
   { "ResampleMethod", FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clResampleMethod, (APTR)IMAGEFX_GET_ResampleMethod, (APTR)IMAGEFX_SET_ResampleMethod },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_imagefx(void)
{
   return(CreateObject(ID_METACLASS, 0, &clImageFX,
      FID_BaseClassID|TLONG, ID_FILTEREFFECT,
      FID_SubClassID|TLONG,  ID_IMAGEFX,
      FID_Name|TSTRING,      "ImageFX",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,      clImageFXActions,
      FID_Fields|TARRAY,     clImageFXFields,
      FID_Size|TLONG,        sizeof(objImageFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
