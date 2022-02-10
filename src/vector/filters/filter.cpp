/*****************************************************************************

Please note that this is not an extension of the Vector class.  It is used for the purposes of filter definitions only.

-CLASS-
VectorFilter: Filters can be applied as post-effects to rendered vectors.

The VectorFilter class allows post-effect filters to be applied to vectors once they have been rendered.  Filter
support is closely modelled around the SVG standard, and effect results are intended to match that of the standard.

Filter instructions are passed to VectorFilter objects via the XML data feed, where they will be parsed into an
internal list of instructions.  It is not possible to modify the instructions once they have been parsed, but they
can be cleared and a new set of instructions can be applied.

It is important to note that filter effects are CPU intensive tasks and real-time performance may be disappointing.
If this is an issue, it can often be rectified by pre-rendering the filter effects in advance and storing the results
in cached bitmaps.

-END-

*****************************************************************************/

static void demultiply_bitmap(objBitmap *);
static ERROR fe_default(objVectorFilter *, VectorEffect *, ULONG, CSTRING);
static VectorEffect * find_effect(objVectorFilter *, CSTRING);
static VectorEffect * find_effect(objVectorFilter *, ULONG);
static ERROR get_bitmap(objVectorFilter *, objBitmap **, UBYTE, ULONG);
static void premultiply_bitmap(objBitmap *);

//****************************************************************************

#include "filter_blur.cpp"
#include "filter_merge.cpp"
#include "filter_composite.cpp"
#include "filter_flood.cpp"
#include "filter_image.cpp"
#include "filter_offset.cpp"
#include "filter_colourmatrix.cpp"
#include "filter_convolve.cpp"
#include "filter_turbulence.cpp"
#include "filter_morphology.cpp"

//****************************************************************************

VectorEffect::VectorEffect()
{
   Source  = VSF_GRAPHIC; // Default is SourceGraphic
   Bitmap  = NULL;
   InputID = 0;
   Error   = ERR_Okay;
   Blank   = false;
}

//****************************************************************************

static void premultiply_bitmap(objBitmap *bmp)
{
   const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   const UBYTE R = bmp->ColourFormat->RedPos>>3;
   const UBYTE G = bmp->ColourFormat->GreenPos>>3;
   const UBYTE B = bmp->ColourFormat->BluePos>>3;

   const ULONG w = (ULONG)(bmp->Clip.Right - bmp->Clip.Left);
   const ULONG h = (ULONG)(bmp->Clip.Bottom - bmp->Clip.Top);

   UBYTE *data = bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth);

   for (ULONG y=0; y < h; y++) {
      UBYTE *pixel = data + (bmp->LineWidth * y);
      for (ULONG x=0; x < w; x++) {
         UBYTE a = pixel[A];
         if (a < 0xff) {
             if (a == 0) pixel[R] = pixel[G] = pixel[B] = 0;
             else {
                pixel[R] = UBYTE((pixel[R] * a + 0xff) >> 8);
                pixel[G] = UBYTE((pixel[G] * a + 0xff) >> 8);
                pixel[B] = UBYTE((pixel[B] * a + 0xff) >> 8);
             }
         }
         pixel += 4;
      }
   }
}

//****************************************************************************

static void demultiply_bitmap(objBitmap *bmp)
{
   const UBYTE A = bmp->ColourFormat->AlphaPos>>3;
   const UBYTE R = bmp->ColourFormat->RedPos>>3;
   const UBYTE G = bmp->ColourFormat->GreenPos>>3;
   const UBYTE B = bmp->ColourFormat->BluePos>>3;

   const ULONG w = (ULONG)(bmp->Clip.Right - bmp->Clip.Left);
   const ULONG h = (ULONG)(bmp->Clip.Bottom - bmp->Clip.Top);

   UBYTE *data = bmp->Data + (bmp->Clip.Left<<2) + (bmp->Clip.Top * bmp->LineWidth);

   for (ULONG y=0; y < h; y++) {
      UBYTE *pixel = data + (bmp->LineWidth * y);
      for (ULONG x=0; x < w; x++) {
         UBYTE a = pixel[A];
         if (a < 0xff) {
            if (a == 0) pixel[R] = pixel[G] = pixel[B] = 0;
            else {
               ULONG r = (ULONG(pixel[R]) * 0xff) / a;
               ULONG g = (ULONG(pixel[G]) * 0xff) / a;
               ULONG b = (ULONG(pixel[B]) * 0xff) / a;
               pixel[R] = UBYTE((r > 0xff) ? 0xff : r);
               pixel[G] = UBYTE((g > 0xff) ? 0xff : g);
               pixel[B] = UBYTE((b > 0xff) ? 0xff : b);
            }
         }
         pixel += 4;
      }
   }
}

//****************************************************************************
// Retrieve a bitmap that is associated with the effect.

static ERROR get_bitmap(objVectorFilter *Self, objBitmap **BitmapResult, UBYTE Source, ULONG InputID)
{
   parasol::Log log(__FUNCTION__);
   //log.trace("Effect: %p, Size: %dx%d", Effect, width, height);

   // Retrieve a bitmap from the bank.  In order to save memory, bitmap data is managed internally so that it always
   // reflects the size of the clipping region.

   LONG bi = Self->BankIndex;
   if (bi >= ARRAYSIZE(Self->Bank)) return ERR_ArrayFull;

   objBitmap *bmp;
   if (!Self->Bank[bi].Bitmap) {
      // NB: Width and Height can be large because the clip region will be defining the true size,
      // and no data is allocated.
      if (CreateObject(ID_BITMAP, NF_INTEGRAL, &bmp,
            FID_Name|TSTR,          "EffectBitmap",
            FID_Width|TLONG,        10000,
            FID_Height|TLONG,       10000,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        BMF_ALPHA_CHANNEL|BMF_NO_DATA,
            TAGEND)) return ERR_CreateObject;
      Self->Bank[bi].Bitmap = bmp;
   }
   else bmp = Self->Bank[bi].Bitmap;

   *BitmapResult = bmp;

   bmp->Clip = Self->SrcBitmap->Clip; // Filter shares the same clip region as the SourceGraphic

   if (bmp->Clip.Right > bmp->Width) bmp->Clip.Right = bmp->Width;
   if (bmp->Clip.Bottom > bmp->Height) bmp->Clip.Bottom = bmp->Height;

   const LONG canvas_width  = bmp->Clip.Right - bmp->Clip.Left;
   const LONG canvas_height = bmp->Clip.Bottom - bmp->Clip.Top;
   bmp->LineWidth = canvas_width * bmp->BytesPerPixel;

   if ((Self->Bank[bi].Data) and (Self->Bank[bi].DataSize < bmp->LineWidth * canvas_height)) {
      FreeResource(Self->Bank[bi].Data);
      Self->Bank[bi].Data = NULL;
      bmp->Data = NULL;
   }

   if (!bmp->Data) {
      if (!AllocMemory(bmp->LineWidth * canvas_height, MEM_DATA|MEM_NO_CLEAR, &Self->Bank[bi].Data, NULL)) {
         Self->Bank[bi].DataSize = bmp->LineWidth * canvas_height;
      }
   }

   bmp->Data = Self->Bank[bi].Data - (bmp->Clip.Left * bmp->BytesPerPixel) - (bmp->Clip.Top * bmp->LineWidth);

   Self->BankIndex++;

   // Copy across the referenced content.

   if (Source IS VSF_GRAPHIC) {
      //bmpDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0x00000000, BAF_FILL);
      gfxCopyArea(Self->SrcBitmap, bmp, 0, 0, 0, Self->SrcBitmap->Width, Self->SrcBitmap->Height, 0, 0);
      if (Self->ColourSpace IS CS_LINEAR_RGB) rgb2linear(*bmp);
   }
   else if (Source IS VSF_ALPHA) {
      //bmpDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0x00000000, BAF_FILL);
      LONG dy = bmp->Clip.Top;
      for (LONG sy=Self->SrcBitmap->Clip.Top; sy < Self->SrcBitmap->Clip.Bottom; sy++) {
         ULONG *src = (ULONG *)(Self->SrcBitmap->Data + (sy * Self->SrcBitmap->LineWidth));
         ULONG *dest = (ULONG *)(bmp->Data + (dy * bmp->LineWidth));
         LONG dx = bmp->Clip.Left;
         for (LONG sx=Self->SrcBitmap->Clip.Left; sx < Self->SrcBitmap->Clip.Right; sx++) {
            dest[dx++] = src[sx] & 0xff000000;
         }
         dy++;
      }
   }
   else if (Source IS VSF_BKGD) { // "Represents an image snapshot of the canvas under the filter region at the time that the filter element is invoked."
      //gfxDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0x00000000, BAF_FILL);
      if ((Self->BkgdBitmap) and (Self->BkgdBitmap->Flags & BMF_ALPHA_CHANNEL)) {
         gfxCopyArea(Self->BkgdBitmap, bmp, 0, Self->BoundX, Self->BoundY, Self->BoundWidth, Self->BoundHeight,
           bmp->Clip.Left, bmp->Clip.Top);
         if (Self->ColourSpace IS CS_LINEAR_RGB) rgb2linear(*bmp);
      }
   }
   else if (Source IS VSF_BKGD_ALPHA) {
      //gfxDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0x00000000, BAF_FILL);
      if ((Self->BkgdBitmap) and (Self->BkgdBitmap->Flags & BMF_ALPHA_CHANNEL)) {
         LONG dy = bmp->Clip.Top;
         for (LONG sy=Self->BkgdBitmap->Clip.Top; sy < Self->BkgdBitmap->Clip.Bottom; sy++) {
            ULONG *src = (ULONG *)(Self->BkgdBitmap->Data + (sy * Self->BkgdBitmap->LineWidth));
            ULONG *dest = (ULONG *)(bmp->Data + (dy * bmp->LineWidth));
            LONG dx = bmp->Clip.Left;
            for (LONG sx=Self->BkgdBitmap->Clip.Left; sx < Self->BkgdBitmap->Clip.Right; sx++) {
               dest[dx++] = src[sx] & 0xff000000;
            }
            dy++;
         }
      }
   }
   else if ((Source IS VSF_REFERENCE) and (InputID)) {
      auto input = find_effect(Self, InputID);
      if (input) {
         if (input->Bitmap) {
            //bmpDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0x00000000, BAF_FILL);
            gfxCopyArea(input->Bitmap, bmp, 0, 0, 0, input->Bitmap->Width, input->Bitmap->Height, 0, 0);
         }
         else log.warning("Referenced filter has no bitmap.");
      }
   }
   else if (Source IS VSF_IGNORE) {
      *BitmapResult = NULL;
      return ERR_Continue;
   }
   else {
      log.warning("Effect source %d is not supported.", Source);
      return ERR_Failed;
   }

   return ERR_Okay;
}

//****************************************************************************

static VectorEffect * find_effect(objVectorFilter *Self, CSTRING Name)
{
   ULONG id = StrHash(Name, TRUE);
   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      if (e->ID IS id) return e;
   }
   parasol::Log log(__FUNCTION__);
   log.warning("Failed to find effect '%s'", Name);
   return NULL;
}

//****************************************************************************

static VectorEffect * find_effect(objVectorFilter *Self, ULONG ID)
{
   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      if (e->ID IS ID) return e;
   }
   return NULL;
}

//****************************************************************************
// Determine the usage count of each effect (i.e. the total number of times the effect is referenced in the pipeline).

static void calc_usage(objVectorFilter *Self)
{
   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      e->UsageCount = 0;
   }

   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      if (e->InputID) {
         auto ref = find_effect(Self, e->InputID);
         if (ref) ref->UsageCount++;
      }
   }
}

//****************************************************************************
// Parser for common filter attributes.

static ERROR fe_default(objVectorFilter *Filter, VectorEffect *Effect, ULONG ID, CSTRING Value)
{
   switch (ID) {
      case SVF_IN: {
         switch (StrHash(Value, FALSE)) {
            case SVF_SOURCEGRAPHIC:   Effect->Source = VSF_GRAPHIC; break;
            case SVF_SOURCEALPHA:     Effect->Source = VSF_ALPHA; break;
            case SVF_BACKGROUNDIMAGE: Effect->Source = VSF_BKGD; break;
            case SVF_BACKGROUNDALPHA: Effect->Source = VSF_BKGD_ALPHA; break;
            case SVF_FILLPAINT:       Effect->Source = VSF_FILL; break;
            case SVF_STROKEPAINT:     Effect->Source = VSF_STROKE; break;
            default:  {
               VectorEffect *e;
               if ((e = find_effect(Filter, Value))) {
                  if (e != Effect) {
                     Effect->Source = VSF_REFERENCE;
                     Effect->InputID = e->ID;
                  }
               }
               break;
            }
         }
         break;
      }

      case SVF_RESULT: // Name the filter.  This allows another filter to use the result as 'in' and create a pipeline
         Effect->ID = StrHash(Value, TRUE); // NB: Case sensitive
         break;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Clears all filter instructions from the object.
-END-
*****************************************************************************/

static ERROR VECTORFILTER_Clear(objVectorFilter *Self, APTR Void)
{
   Self->Effects.clear();

   for (LONG i=0; i < ARRAYSIZE(Self->Bank); i++) {
      if (Self->Bank[i].Bitmap) { acFree(Self->Bank[i].Bitmap); Self->Bank[i].Bitmap = NULL; }
      if (Self->Bank[i].Data) { FreeResource(Self->Bank[i].Data); Self->Bank[i].Data = NULL; }
   }

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
DataFeed: Filter effects are parsed via the DataFeed action.

Filter instructions can be added to a VectorFilter object by parsing them as an SVG document.  The instructions must be
passed as a sequential list that is formatted to SVG guidelines.

The following example illustrates:

<pre>
&lt;feOffset in="BackgroundImage" dx="0" dy="125"/&gt;
&lt;feGaussianBlur stdDeviation="8" result="blur"/&gt;
&lt;feMerge&gt;
  &lt;feMergeNode in="blur"/&gt;
  &lt;feMergeNode in="SourceGraphic"/&gt;
&lt;/feMerge&gt;
</pre>

Unsupported or invalid elements will be reported in debug output and then ignored, allowing the parser to process
as many instructions as possible.

The XML object that is used to parse the effects is accessible through the #EffectXML field.

-END-

*****************************************************************************/

static ERROR VECTORFILTER_DataFeed(objVectorFilter *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   ERROR error = ERR_Okay;
   if (Args->DataType IS DATA_XML) {
      if (Self->EffectXML) { acFree(Self->EffectXML); Self->EffectXML = NULL; }

      if (!NewObject(ID_XML, NF_INTEGRAL, &Self->EffectXML)) {
         SetString(Self->EffectXML, FID_Statement, (CSTRING)Args->Buffer);
         if (!acInit(Self->EffectXML)) {
            for (auto tag = Self->EffectXML->Tags[0]; tag; tag=tag->Next) {
               log.trace("Processing filter element '%s'", tag->Attrib->Name);
               switch(StrHash(tag->Attrib->Name, FALSE)) {
                  case SVF_FEBLUR:           Self->Effects.emplace_back(std::make_unique<BlurEffect>(Self, tag)); break;
                  case SVF_FEGAUSSIANBLUR:   Self->Effects.emplace_back(std::make_unique<BlurEffect>(Self, tag)); break;

                  case SVF_FEOFFSET:         Self->Effects.emplace_back(std::make_unique<OffsetEffect>(Self, tag)); break;
                  case SVF_FEMERGE:          Self->Effects.emplace_back(std::make_unique<MergeEffect>(Self, tag)); break;
                  case SVF_FECOLORMATRIX:    // American spelling
                  case SVF_FECOLOURMATRIX:   Self->Effects.emplace_back(std::make_unique<ColourEffect>(Self, tag)); break;
                  case SVF_FECONVOLVEMATRIX: Self->Effects.emplace_back(std::make_unique<ConvolveEffect>(Self, tag)); break;
                  case SVF_FEBLEND:          // Blend and composite share the same code.
                  case SVF_FECOMPOSITE:      Self->Effects.emplace_back(std::make_unique<CompositeEffect>(Self, tag)); break;
                  case SVF_FEFLOOD:          Self->Effects.emplace_back(std::make_unique<FloodEffect>(Self, tag)); break;
                  case SVF_FETURBULENCE:     Self->Effects.emplace_back(std::make_unique<TurbulenceEffect>(Self, tag)); break;
                  case SVF_FEMORPHOLOGY:     Self->Effects.emplace_back(std::make_unique<MorphEffect>(Self, tag)); break;
                  case SVF_FEIMAGE:          Self->Effects.emplace_back(std::make_unique<ImageEffect>(Self, tag)); break;
                  case SVF_FEDISPLACEMENTMAP:
                  case SVF_FETILE:
                  case SVF_FECOMPONENTTRANSFER:
                  case SVF_FEDIFFUSELIGHTING:
                  case SVF_FESPECULARLIGHTING:
                  case SVF_FEDISTANTLIGHT:
                  case SVF_FEPOINTLIGHT:
                  case SVF_FESPOTLIGHT:
                     log.warning("Filter element '%s' is not currently supported.", tag->Attrib->Name);
                     break;

                  default:
                     log.warning("Filter element '%s' not recognised.", tag->Attrib->Name);
                     break;
               }

               if (error) log.warning("Failed to process filter element '%s'", tag->Attrib->Name);
            }

            calc_usage(Self);
         }
         else {
            acFree(Self->EffectXML);
            Self->EffectXML = NULL;
            error = ERR_Init;
         }
      }
      else error = ERR_NewObject;
   }

   return error;
}

//****************************************************************************

static ERROR VECTORFILTER_Draw(objVectorFilter *Self, struct acDraw *Args)
{
   parasol::Log log;

   if ((!Self->Scene) or (!Self->Viewport)) {
      log.trace("Scene and/or Viewport not defined.");
      return log.warning(ERR_FieldNotSet);
   }

   if (!Self->Viewport->Child) { log.warning("Target vector not defined."); return ERR_FieldNotSet; }

   Self->Rendered = false;
   Self->DrawStamp = PreciseTime();

   // The target bitmap will mirror the size of the vector's nearest viewport.

   auto child = (objVector *)Self->Viewport->Child;

   SetFields(Self->Scene,
      FID_PageWidth|TDOUBLE,  (DOUBLE)child->ParentView->vpFixedWidth,
      FID_PageHeight|TDOUBLE, (DOUBLE)child->ParentView->vpFixedHeight,
      TAGEND);

   // TODO: Although the scene and viewport should be identical, we should still pull these values from the viewport.
   LONG page_width = Self->Scene->PageWidth;
   LONG page_height = Self->Scene->PageHeight;

   if ((page_width < 1) or (page_height < 1)) return ERR_Okay;
   if (page_width > 4096) page_width = 4096;
   if (page_height > 4096) page_height = 4096;

   // Calculate the area that will be affected by the filter algorithms.  The area will be reflected in the target
   // Bitmap's clipping coordinates.

   auto save_vector = Self->Viewport->Child->Next; // Switch off the Next pointer to prevent drawing siblings.
   Self->Viewport->Child->Next = NULL;

   child->Filter = NULL; // Temporarily turning off the filter is required to prevent infinite recursion.

   UBYTE bound = FALSE;
   if (Self->Units IS VUNIT_BOUNDING_BOX) {
      // All coordinates are relative to the target vector, or vectors if we are applied to a group.

      std::array<DOUBLE, 4> bounds = { (DOUBLE)Self->Scene->PageWidth, (DOUBLE)Self->Scene->PageHeight, 0, 0 };
      calc_full_boundary((objVector *)Self->Viewport->Child, bounds);

      if ((bounds[2] <= bounds[0]) or (bounds[3] <= bounds[1])) {
         log.warning("Unable to draw filter as no child vector produces a path.");
         return ERR_Failed;
      }

      if (Self->Dimensions & DMF_FIXED_X) Self->BoundX = bounds[0] + Self->X;
      else if (Self->Dimensions & DMF_RELATIVE_X) Self->BoundX = bounds[0] + (Self->X * (bounds[2] - bounds[0]));
      else Self->BoundX = bounds[0];

      if (Self->Dimensions & DMF_FIXED_Y) Self->BoundY = bounds[1] + Self->Y;
      else if (Self->Dimensions & DMF_RELATIVE_Y) Self->BoundY = bounds[1] + (Self->Y * (bounds[3] - bounds[1]));
      else Self->BoundY = bounds[1];

      if (Self->Dimensions & DMF_FIXED_WIDTH) Self->BoundWidth = Self->Width;
      else if (Self->Dimensions & DMF_RELATIVE_WIDTH) Self->BoundWidth = Self->Width * (bounds[2] - bounds[0]);
      else Self->BoundWidth = bounds[2] - bounds[0];

      if (Self->Dimensions & DMF_FIXED_HEIGHT) Self->BoundHeight = Self->Height;
      else if (Self->Dimensions & DMF_RELATIVE_HEIGHT) Self->BoundHeight = Self->Height * (bounds[3] - bounds[1]);
      else Self->BoundHeight = bounds[3] = bounds[1];

      Self->ViewX = Self->Viewport->vpFixedRelX;
      Self->ViewY = Self->Viewport->vpFixedRelY;
      Self->ViewWidth  = Self->Viewport->vpFixedWidth;
      Self->ViewHeight = Self->Viewport->vpFixedHeight;
      bound = TRUE;
   }
   else {
      // Use page relative coordinates.  In this mode we have to allocate a rectangular path that matches the filter's
      // dimensions, then apply transforms to it if necessary.

      DOUBLE fx, fy, fw, fh;

      if (Self->Dimensions & DMF_FIXED_X) fx = Self->X;
      else if (Self->Dimensions & DMF_RELATIVE_X) fx = Self->X * (DOUBLE)page_width;
      else fy = 0;

      if (Self->Dimensions & DMF_FIXED_Y) fy = Self->Y;
      else if (Self->Dimensions & DMF_RELATIVE_Y) fy = Self->Y * (DOUBLE)page_height;
      else fx = 0;

      if (Self->Dimensions & DMF_FIXED_WIDTH) fw = Self->Width;
      else if (Self->Dimensions & DMF_RELATIVE_WIDTH) fw = Self->Width * (DOUBLE)page_width;
      else fw = page_width;

      if (Self->Dimensions & DMF_FIXED_HEIGHT) fh = Self->Height;
      else if (Self->Dimensions & DMF_RELATIVE_HEIGHT) fh = Self->Height * (DOUBLE)page_height;
      else fh = page_height;

      // Sometimes transforms need to be applied, e.g. <g transform="translate(...)">
      if (child->Matrices) {
         agg::path_storage rect;
         rect.move_to(fx, fy);
         rect.line_to(fx+fw, fy);
         rect.line_to(fx+fw, fy+fh);
         rect.line_to(fx, fy+fh);
         rect.close_polygon();

         agg::trans_affine transform;
         transform.tx += fx;
         transform.ty += fy;
         compile_transforms(*child, transform);
         rect.transform(transform);

         bounding_rect_single(rect, 0, &fx, &fy, &fw, &fh);
         fw -= fx;
         fh -= fy;
      }

      Self->BoundX = fx;
      Self->BoundY = fy;
      Self->BoundWidth = fw;
      Self->BoundHeight = fh;

      // If the filter is in user-space mode then the target viewport is taken as the provided x,y,width,height values
      // after transformation.

      Self->ViewX = fx;
      Self->ViewY = fy;
      Self->ViewWidth  = fw;
      Self->ViewHeight = fh;
   }

   // Render the vector to an internal bitmap that we will use as SourceGraphic and SourceAlpha input.

   if (!Self->SrcBitmap) {
      if (CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->SrcBitmap,
            FID_Name|TSTR,          "SourceGraphic",
            FID_Width|TLONG,        Self->BoundX + Self->BoundWidth,
            FID_Height|TLONG,       Self->BoundY + Self->BoundHeight,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
            TAGEND)) return ERR_CreateObject;
      Self->Bank[0].Bitmap = Self->SrcBitmap;
   }
   else if ((Self->BoundX + Self->BoundWidth > Self->SrcBitmap->Width) or
            (Self->BoundY + Self->BoundHeight > Self->SrcBitmap->Height)) {
      acResize(Self->SrcBitmap, Self->BoundX + Self->BoundWidth, Self->BoundY + Self->BoundHeight, 32);
   }

   Self->SrcBitmap->Clip.Left   = Self->BoundX;
   Self->SrcBitmap->Clip.Top    = Self->BoundY;
   Self->SrcBitmap->Clip.Right  = Self->BoundX + Self->BoundWidth;
   Self->SrcBitmap->Clip.Bottom = Self->BoundY + Self->BoundHeight;
   if (Self->SrcBitmap->Clip.Left < 0) Self->SrcBitmap->Clip.Left = 0;
   if (Self->SrcBitmap->Clip.Top < 0) Self->SrcBitmap->Clip.Top = 0;
   if (Self->SrcBitmap->Clip.Right > Self->SrcBitmap->Width) Self->SrcBitmap->Clip.Right = Self->SrcBitmap->Width;
   if (Self->SrcBitmap->Clip.Bottom > Self->SrcBitmap->Height) Self->SrcBitmap->Clip.Bottom = Self->SrcBitmap->Height;

   gfxDrawRectangle(Self->SrcBitmap, 0, 0, Self->SrcBitmap->Width, Self->SrcBitmap->Height, 0x00000000, BAF_FILL);

   Self->BankIndex = 1;
   Self->Scene->Bitmap = Self->SrcBitmap;
   acDraw(Self->Scene);

   child->Filter = Self;
   child->Next = save_vector;

   // Now apply the effects to the rendered scene

   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      if (e->Blank) continue; // Ignore effects that don't produce graphics

      ERROR error = ERR_Okay;
      e->DestX = e->XOffset;
      e->DestY = e->YOffset;
      e->Bitmap = NULL;

      if (e->InputID) {
         auto input = find_effect(Self, e->InputID);

         if (input) { // Offset inheritance from the input
            e->DestX += input->DestX;
            e->DestY += input->DestY;

            input->applyInput(*e);

            if ((e->Source IS VSF_REFERENCE) and (input->UsageCount IS 1) and (input->Bitmap)) {
               e->Bitmap = input->Bitmap;
            }
         }
      }

      // If we inherit the bitmap from another effect, try and use it rather than copying a new bitmap from scratch.

      if (!e->Bitmap) error = get_bitmap(Self, &e->Bitmap, e->Source, 0);

      if (!error) e->apply(Self);
      else if (error != ERR_Continue) log.warning("Failed to configure bitmap for effect.");
   }

   // Render the filter results to the destination bitmap

   if (!Self->Rendered) {
      Self->Rendered = true;
      objBitmap *bmp;
      for (auto &ptr : Self->Effects) {
         auto e = ptr.get();
         if ((e->ID) and (e->UsageCount > 0)) continue; // Don't draw the effect if it's being piped to something else.
         if ((bmp = e->Bitmap)) { // Note that blank effects don't generate bitmaps
            if (Self->ColourSpace IS CS_LINEAR_RGB) linear2RGB(*bmp);
            if (Self->Opacity < 1.0) bmp->Opacity = 255.0 * Self->Opacity;
            gfxCopyArea(bmp, Self->BkgdBitmap, BAF_BLEND|BAF_COPY, 0, 0, bmp->Width, bmp->Height, 0, 0);
            bmp->Opacity = 255;
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORFILTER_Free(objVectorFilter *Self, APTR Void)
{
   VECTORFILTER_Clear(Self, NULL);

   Self->Effects.~vector();

   if (Self->EffectXML) { acFree(Self->EffectXML);  Self->EffectXML = NULL; }
   if (Self->Scene)     { acFree(Self->Scene);      Self->Scene = NULL; }
   if (Self->Path)      { FreeResource(Self->Path); Self->Path = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORFILTER_Init(objVectorFilter *Self, APTR Void)
{
   parasol::Log log(__FUNCTION__);

   if ((Self->Units <= 0) or (Self->Units >= VUNIT_END)) {
      log.traceWarning("Invalid Units value of %d", Self->Units);
      return log.warning(ERR_OutOfRange);
   }

   if (acInit(Self->Scene)) return ERR_Init;
   if (acInit(Self->Viewport)) return ERR_Init;
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORFILTER_NewObject(objVectorFilter *Self, APTR Void)
{
   if (!NewObject(ID_VECTORSCENE, NF_INTEGRAL, &Self->Scene)) {
      if (!NewObject(ID_VECTORVIEWPORT, 0, &Self->Viewport)) {
         SetOwner(Self->Viewport, Self->Scene);
         new (&Self->Effects) std::vector<VectorEffect>;
         Self->Scene->PageWidth  = 1;
         Self->Scene->PageHeight = 1;
         Self->Units          = VUNIT_BOUNDING_BOX;
         Self->PrimitiveUnits = VUNIT_UNDEFINED;
         Self->Opacity        = 1.0;
         Self->X              = -0.1;
         Self->Y              = -0.1;
         Self->Width          = 1.2;
         Self->Height         = 1.2;
         Self->ColourSpace    = CS_SRGB; // Our preferred colour-space is sRGB for speed.  Note that the SVG class will change this to linear by default.
         Self->Dimensions     = DMF_RELATIVE_X|DMF_RELATIVE_Y|DMF_RELATIVE_WIDTH|DMF_RELATIVE_HEIGHT;
         return ERR_Okay;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************

-FIELD-
ColourSpace: The colour space of the filter graphics (SRGB or linear RGB).

By default, colour filters are processed in SRGB format.  This is the same colour space as used by the rest of the
graphics system, which means that no special conversion is necessary prior to and post filter processing.  However,
linear RGB is better suited for producing high quality results at a cost of speed.

Note that if SVG compatibility is required, linear RGB must be used as the default.

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_X">The #X value is a fixed coordinate.</>
<type name="FIXED_Y">The #Y value is a fixed coordinate.</>
<type name="RELATIVE_X">The #X value is a relative coordinate.</>
<type name="RELATIVE_Y">The #Y value is a relative coordinate.</>
<type name="FIXED_WIDTH">The #Width value is a fixed coordinate.</>
<type name="FIXED_HEIGHT">The #Height value is a fixed coordinate.</>
<type name="RELATIVE_WIDTH">The #Width value is a relative coordinate.</>
<type name="RELATIVE_HEIGHT">The #Height value is a relative coordinate.</>
</types>

-FIELD-
Height: The height of the filter area.  Can be expressed as a fixed or relative coordinate.

The height of the filter area is expressed here as a fixed or relative coordinate.
-END-

*****************************************************************************/

static ERROR VECTORFILTER_GET_Height(objVectorFilter *Self, struct Variable *Value)
{
   DOUBLE val = Self->Height;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = val;
   return ERR_Okay;
}

static ERROR VECTORFILTER_SET_Height(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (val > 0) {
      if (Value->Type & FD_PERCENTAGE) {
         val = val * 0.01;
         Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_HEIGHT) & (~DMF_FIXED_HEIGHT);
      }
      else Self->Dimensions = (Self->Dimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);

      Self->Height = val;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
-FIELD-
Inherit: Inherit attributes from a VectorFilter referenced here.

Attributes can be inherited from another filter by referencing that gradient in this field.  This feature is provided
primarily for the purpose of simplifying SVG compatibility and its use may result in an unnecessary performance penalty.

*****************************************************************************/

static ERROR VECTORFILTER_SET_Inherit(objVectorFilter *Self, objVectorFilter *Value)
{
   if (Value) {
      if (Value->Head.ClassID IS ID_VECTORFILTER) Self->Inherit = Value;
      else return ERR_InvalidValue;
   }
   else Self->Inherit = NULL;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Opacity: The opacity of the filter.

The opacity of the filter is defined as a value between 0.0 and 1.0, with 1.0 being fully opaque.  The default value
is 1.0.

*****************************************************************************/

static ERROR VECTORFILTER_SET_Opacity(objVectorFilter *Self, DOUBLE Value)
{
   if (Value < 0.0) Value = 0;
   else if (Value > 1.0) Value = 1.0;
   Self->Opacity = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Path: Affix this path to all file references in the filter definition.

Setting the Path field is recommended if the filter contains sub-classes that make file references, such as
a filter image.  Any relative file reference will be prefixed with the path string that is specified here.

*****************************************************************************/

static ERROR VECTORFILTER_SET_Path(objVectorFilter *Self, CSTRING Value)
{
   if ((Value) and (*Value)) {
      // Setting a path of "my/house/is/red.svg" results in "my/house/is/"

      STRING str;
      if (!ResolvePath(Value, RSF_NO_FILE_CHECK, &str)) {
         WORD last = 0;
         for (WORD i=0; str[i]; i++) {
            if ((str[i] IS '/') or (str[i] IS '\\')) last = i + 1;
         }
         str[last] = 0;

         if (Self->Path) FreeResource(Self->Path);
         Self->Path = str;
      }
      else return ERR_ResolvePath;
   }
   else {
      if (Self->Path) FreeResource(Self->Path);
      Self->Path = NULL;
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
PrimitiveUnits: Private. Not currently implemented.

-FIELD-
Scene: Refers to the internal @VectorScene that will be used for filter processing.

The Filter class will allocate a @VectorScene object on initialisation and use it for hosting filter
processing.  The dimensions of the scene will match that of the area hosting the filter.  The scene object must
not be modified as it is managed entirely by the filter.

-FIELD-
Units: Defines the coordinate system for fields X, Y, Width and Height.

The default coordinate system for gradients is `BOUNDING_BOX`, which positions the filter around the vector that
references it.  The alternative is `USERSPACE`, which positions the filter relative to the current viewport.

-FIELD-
Vector: Private. Must refer to a vector that will be processed through the filter.  Refer to draw_vectors().
-END-

*****************************************************************************/

static ERROR VECTORFILTER_SET_Vector(objVectorFilter *Self, objVector *Value)
{
   if (Self->Viewport) {
      if (Value->Head.ClassID IS ID_VECTOR) {
         Self->Viewport->Child = Value;
         return ERR_Okay;
      }
      else return ERR_InvalidValue;
   }
   else return ERR_NotInitialised;
}

/*****************************************************************************

-FIELD-
Viewport: Refers to a VectorViewport object allocated during initialisation.

The VectorFilter will allocate a @VectorViewport on initialisation to manage its content.

-FIELD-
Width: The width of the filter area.  Can be expressed as a fixed or relative coordinate.

The width of the filter area is expressed here as a fixed or relative coordinate.
-END-

*****************************************************************************/

static ERROR VECTORFILTER_GET_Width(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val = Self->Width;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = val;
   return ERR_Okay;
}

static ERROR VECTORFILTER_SET_Width(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (val > 0) {
      if (Value->Type & FD_PERCENTAGE) {
         val = val * 0.01;
         Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_WIDTH) & (~DMF_FIXED_WIDTH);
      }
      else Self->Dimensions = (Self->Dimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);

      Self->Width = val;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
-FIELD-
X: X coordinate for the filter.

The (X,Y) field values define the starting coordinate for mapping filters.
-END-
*****************************************************************************/

static ERROR VECTORFILTER_GET_X(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val = Self->X;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = val;
   return ERR_Okay;
}

static ERROR VECTORFILTER_SET_X(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_X) & (~DMF_FIXED_X);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);

   Self->X = val;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Y: Y coordinate for the filter.

The (X,Y) field values define the starting coordinate for mapping filters.
-END-
*****************************************************************************/

static ERROR VECTORFILTER_GET_Y(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val = Self->Y;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = val;
   return ERR_Okay;
}

static ERROR VECTORFILTER_SET_Y(objVectorFilter *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_Y) & (~DMF_FIXED_Y);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);

   Self->Y = val;
   return ERR_Okay;
}

//****************************************************************************

static const FieldDef clFilterDimensions[] = {
   { "FixedX",         DMF_FIXED_X },
   { "FixedY",         DMF_FIXED_Y },
   { "RelativeX",      DMF_RELATIVE_X },
   { "RelativeY",      DMF_RELATIVE_Y },
   { "FixedWidth",     DMF_FIXED_WIDTH },
   { "FixedHeight",    DMF_FIXED_HEIGHT },
   { "RelativeWidth",  DMF_RELATIVE_WIDTH },
   { "RelativeHeight", DMF_RELATIVE_HEIGHT },
   { NULL, 0 }
};

#include "filter_def.c"

static const FieldArray clFilterFields[] = {
   { "X",              FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VECTORFILTER_GET_X, (APTR)VECTORFILTER_SET_X },
   { "Y",              FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VECTORFILTER_GET_Y, (APTR)VECTORFILTER_SET_Y },
   { "Width",          FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VECTORFILTER_GET_Width, (APTR)VECTORFILTER_SET_Width },
   { "Height",         FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)VECTORFILTER_GET_Height, (APTR)VECTORFILTER_SET_Height },
   { "Opacity",        FDF_DOUBLE|FDF_RW,           0, NULL, (APTR)VECTORFILTER_SET_Opacity },
   { "Scene",          FDF_INTEGRAL|FDF_R,          0, NULL, NULL },
   { "Viewport",       FDF_SYSTEM|FDF_OBJECT|FDF_R, 0, NULL, NULL },
   { "Inherit",        FDF_OBJECT|FDF_RW,           0, NULL, (APTR)VECTORFILTER_SET_Inherit },
   { "EffectXML",      FDF_OBJECT|FDF_R,            ID_XML, NULL, NULL },
   { "Units",          FDF_LONG|FDF_LOOKUP|FDF_RW,  (MAXINT)&clVectorFilterUnits, NULL, NULL },
   { "PrimitiveUnits", FDF_LONG|FDF_LOOKUP|FDF_RW,  (MAXINT)&clVectorFilterPrimitiveUnits, NULL, NULL },
   { "Dimensions",     FDF_LONGFLAGS|FDF_R,         (MAXINT)&clFilterDimensions, NULL, NULL },
   { "ColourSpace",    FDF_LONG|FDF_LOOKUP|FDF_RW,  (MAXINT)&clVectorFilterColourSpace, NULL, NULL },
   // Virtual fields
   { "Vector",         FDF_SYSTEM|FDF_VIRTUAL|FDF_OBJECT|FDF_W, 0, NULL, (APTR)VECTORFILTER_SET_Vector },
   { "Path",           FDF_VIRTUAL|FDF_STRING|FDF_W, 0, NULL, (APTR)VECTORFILTER_SET_Path },
   END_FIELD
};

static ERROR init_filter(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorFilter,
      FID_BaseClassID|TLONG, ID_VECTORFILTER,
      FID_Name|TSTRING,      "VectorFilter",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,      clVectorFilterActions,
      FID_Fields|TARRAY,     clFilterFields,
      FID_Size|TLONG,        sizeof(objVectorFilter),
      FID_Path|TSTR,         "modules:vector",
      TAGEND));
}

