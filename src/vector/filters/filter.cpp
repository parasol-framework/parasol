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
static ERROR get_source_bitmap(objVectorFilter *, objBitmap **, UBYTE, ULONG, bool);
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
   SourceType = VSF_GRAPHIC; // Default is SourceGraphic
   MixType = 0;
   OutBitmap = NULL;
   InputID = 0;
   MixID   = 0;
   Error   = ERR_Okay;
   Blank   = false;
}

//****************************************************************************
// Pre-multiplying affects RGB channels where alpha masking is present.  The alpha values are unmodified.
//
// It is not necessary to pre-multiply if a processing effect is only utilising the alpha channel as an input.

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
// Where possible, demultiplying should be avoided as it requires numeric division.

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
// Return a bitmap from the bank.  In order to save memory, bitmap data is managed internally so that it always
// reflects the size of the clipping region.

static ERROR get_banked_bitmap(objVectorFilter *Self, objBitmap **BitmapResult)
{
   LONG bi = Self->BankIndex;
   if (bi >= ARRAYSIZE(Self->Bank)) return ERR_ArrayFull;

   objBitmap *bmp;
   if (!Self->Bank[bi].Bitmap) {
      // NB: The clip region defines the true size and no data is allocated by the bitmap itself.
      char name[32];
      StrFormat(name, sizeof(name), "dummy_fx_bitmap_%d", bi);
      if (CreateObject(ID_BITMAP, NF_INTEGRAL, &bmp,
            FID_Name|TSTR,          name,
            FID_Width|TLONG,        Self->VectorClip.Right,
            FID_Height|TLONG,       Self->VectorClip.Bottom,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        BMF_ALPHA_CHANNEL|BMF_NO_DATA,
            TAGEND)) return ERR_CreateObject;
      Self->Bank[bi].Bitmap = bmp;
   }
   else {
      bmp = Self->Bank[bi].Bitmap;
      bmp->Width = Self->VectorClip.Right;
      bmp->Height = Self->VectorClip.Bottom;
   }

   *BitmapResult = bmp;

   bmp->Clip = Self->VectorClip; // Filter shares the same clip region as the client vector

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
   return ERR_Okay;
}

//****************************************************************************
// Returns a rendered bitmap that represents the source.  Where possible, if a bitmap is being referenced then that
// reference will be returned.  Otherwise a new bitmap is allocated and rendered with the effect.  The bitmap must
// not be freed as they are permanently maintained until the VectorFilter is destroyed.

static ERROR get_source_bitmap(objVectorFilter *Self, objBitmap **BitmapResult, UBYTE SourceType, ULONG EffectID, bool Premultiply)
{
   parasol::Log log(__FUNCTION__);

   objBitmap *bmp;
   log.branch("ID: %u, Type: %d", EffectID, SourceType);

   if (SourceType IS VSF_GRAPHIC) { // SourceGraphic: Render the source vector without transformations (transforms will be applied in the final steps).
      if (auto error = get_banked_bitmap(Self, &bmp)) return log.warning(error);
      if (auto sg = get_source_graphic(Self)) {
         gfxCopyArea(sg, bmp, 0, 0, 0, Self->SourceGraphic->Width, Self->SourceGraphic->Height, 0, 0);
         if (Self->ColourSpace IS CS_LINEAR_RGB) rgb2linear(*bmp);
      }
      else log.warning("Unable to retrieve source graphic.");
   }
   else if (SourceType IS VSF_ALPHA) { // SourceAlpha
      if (auto error = get_banked_bitmap(Self, &bmp)) return log.warning(error);
      if (auto sg = get_source_graphic(Self)) {
         LONG dy = bmp->Clip.Top;
         for (LONG sy=sg->Clip.Top; sy < sg->Clip.Bottom; sy++) {
            ULONG *src = (ULONG *)(sg->Data + (sy * sg->LineWidth));
            ULONG *dest = (ULONG *)(bmp->Data + (dy * bmp->LineWidth));
            LONG dx = bmp->Clip.Left;
            for (LONG sx=sg->Clip.Left; sx < sg->Clip.Right; sx++) {
               dest[dx++] = src[sx] & 0xff000000;
            }
            dy++;
         }
      }
      else log.warning("Unable to retrieve source alpha.");
   }
   else if (SourceType IS VSF_BKGD) {
      // "Represents an image snapshot of the canvas under the filter region at the time that the filter element is invoked."
      // NOTE: The client needs to specify 'enable-background' in the nearest container element in order to indicate where the background is coming from;
      // additionally it serves as a marker for graphics to be rendered to a separate bitmap (essential for coping with any transformations in the scene graph).
      //
      // TODO: The code as it exists here is quite basic and needs a fair amount of work in order to meet the above requirements.

      if (auto error = get_banked_bitmap(Self, &bmp)) return log.warning(error);
      if ((Self->BkgdBitmap) and (Self->BkgdBitmap->Flags & BMF_ALPHA_CHANNEL)) {
         gfxCopyArea(Self->BkgdBitmap, bmp, 0, Self->BoundX, Self->BoundY, Self->BoundWidth, Self->BoundHeight,
           bmp->Clip.Left, bmp->Clip.Top);
         if (Self->ColourSpace IS CS_LINEAR_RGB) rgb2linear(*bmp);
      }
   }
   else if (SourceType IS VSF_BKGD_ALPHA) {
      if (auto error = get_banked_bitmap(Self, &bmp)) return log.warning(error);
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
   else if ((SourceType IS VSF_REFERENCE) and (EffectID)) {
      if (auto e = find_effect(Self, EffectID)) {
         bmp = e->OutBitmap;
         if (!bmp) log.warning("Effect %u does not output a bitmap.", EffectID);
      }
      else log.warning("Unable to find effect %u", EffectID);
   }
   else if (SourceType IS VSF_IGNORE) {
      *BitmapResult = NULL;
      return ERR_Continue;
   }
   else {
      log.warning("Effect source %d is not supported.", SourceType);
      return ERR_Failed;
   }

   if (Premultiply) premultiply_bitmap(bmp);

   *BitmapResult = bmp;
   return ERR_Okay;
}

//****************************************************************************
// Render the scene to an internal bitmap that can be used for SourceGraphic and SourceAlpha input.
// If the referenced vector has no content then the result is a bitmap cleared to 0x00000000, as per SVG specs.

objBitmap * get_source_graphic(objVectorFilter *Self)
{
   parasol::Log log(__FUNCTION__);

   if (!Self->ClientVector) {
      log.warning("No ClientVector defined.");
      return NULL;
   }

   if (!Self->SourceGraphic) {
      if (CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->SourceGraphic,
            FID_Name|TSTR,          "source_graphic",
            FID_Width|TLONG,        Self->BoundX + Self->BoundWidth,
            FID_Height|TLONG,       Self->BoundY + Self->BoundHeight,
            FID_BitsPerPixel|TLONG, 32,
            FID_Flags|TLONG,        BMF_ALPHA_CHANNEL,
            TAGEND)) return NULL;

      Self->SourceGraphic->Clip = Self->VectorClip;
   }
   else if ((Self->BoundX + Self->BoundWidth > Self->SourceGraphic->Width) or
            (Self->BoundY + Self->BoundHeight > Self->SourceGraphic->Height)) {
      if (acResize(Self->SourceGraphic, Self->BoundX + Self->BoundWidth, Self->BoundY + Self->BoundHeight, 32) != ERR_Okay)
         return NULL;

      Self->SourceGraphic->Clip = Self->VectorClip;
   }
   else if (Self->Rendered) return Self->SourceGraphic; // Source bitmap already exists and drawn at the correct size.

   if (!Self->Scene) {
      if (!CreateObject(ID_VECTORSCENE, NF_INTEGRAL, &Self->Scene,
            FID_PageWidth|TLONG,  Self->ViewWidth,
            FID_PageHeight|TLONG, Self->ViewHeight,
            TAGEND)) {

         objVectorViewport *viewport;
         if (!CreateObject(ID_VECTORVIEWPORT, 0, &viewport,
               FID_Owner|TLONG, Self->Scene->Head.UID,
               TAGEND)) {
            viewport->Child = Self->ClientVector;
         }
      }
      else return NULL;
   }
   else return NULL;

   auto vector = Self->ClientVector;
   auto save_vector = vector->Next; // Switch off the Next pointer to prevent processing of siblings.
   vector->Next = NULL;
   vector->Filter = NULL; // Temporarily turning off the filter is required to prevent infinite recursion.

   gfxDrawRectangle(Self->SourceGraphic, 0, 0, Self->SourceGraphic->Width, Self->SourceGraphic->Height, 0x00000000, BAF_FILL);
   Self->Scene->Bitmap = Self->SourceGraphic;
   acDraw(Self->Scene);

   vector->Filter = Self;
   vector->Next = save_vector;

   Self->Rendered = true;
   return Self->SourceGraphic;
}

//****************************************************************************
// Rendering process is as follows:
//
// * A VectorScene calls this function with a vector that requested the filter, and a target bitmap.
//   The TargetBitmap doubles as the background because it will already contain rendered content.  (NOTE: May require changes to support enable-background and transforms correctly).
// * Each effect is processed in their original order and each can produce a single OutBitmap.
//   * An effect may request the SourceGraphic, in which case we render the Vector to a separate scene graph and without transforms.
// * Each effect's OutBitmap is rendered to TargetBitmap.  Transforms may be applied at this stage.
//   The overall process is similar to that of drawImage() in scene_draw.cpp.
//
// SPECIAL CASE: SVG rules dictate that if the Vector is a container (i.e. Viewport) then the filter applies to the
// contents of the group as a whole.  The group's children do not render to the screen directly; instead, the
// graphics commands necessary to render the children are stored temporarily. Typically, the graphics commands
// are executed as part of the processing of the referenced ‘filter’ element via use of the keywords
// SourceGraphic or SourceAlpha. Filter effects can be applied to container elements with no content (e.g., an
// empty ‘g’ element), in which case the SourceGraphic or SourceAlpha consist of a transparent black rectangle
// that is the size of the filter effects region.


ERROR render_filter(objVectorFilter *Self, objVector *Vector, objBitmap *TargetBitmap)
{
   parasol::Log log(__FUNCTION__);

   if (!Vector) return log.warning(ERR_NullArgs);

   parasol::SwitchContext context(Self);

   #ifdef DBG_DRAW
      log.traceBranch("Rendering filter for %s.", get_name(Vector));
   #endif

   Self->BkgdBitmap   = TargetBitmap;
   Self->Rendered     = false;
   Self->BankIndex    = 0;
   Self->ClientVector = Vector;

   // Calculate the area that will be affected by the filter algorithms.  The area will be reflected in the target
   // Bitmap's clipping coordinates.

   if (Self->Units IS VUNIT_BOUNDING_BOX) {
      // All coordinates are relative to the client vector, or vectors if we are applied to a group.

      std::array<DOUBLE, 4> bounds = { Vector->ParentView->vpFixedWidth, Vector->ParentView->vpFixedHeight, 0, 0 };
      calc_full_boundary((objVector *)Vector, bounds, false);

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

      Self->ViewX = 0;
      Self->ViewY = 0;
      Self->ViewWidth  = bounds[2] - bounds[0];
      Self->ViewHeight = bounds[3] - bounds[1];

   }
   else { // USERSPACE
      DOUBLE fx, fy, fw, fh;

      LONG page_width  = Vector->ParentView->vpFixedWidth; // Userspace
      LONG page_height = Vector->ParentView->vpFixedHeight;

      if ((page_width < 1) or (page_height < 1)) return ERR_Okay;

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
      if (Vector->Matrices) {
         agg::path_storage rect;
         rect.move_to(fx, fy);
         rect.line_to(fx+fw, fy);
         rect.line_to(fx+fw, fy+fh);
         rect.line_to(fx, fy+fh);
         rect.close_polygon();

         agg::trans_affine transform;
         transform.tx += fx;
         transform.ty += fy;
         apply_transforms(*Vector, transform);
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

   ClipRectangle clip = {
      .Left   = (Self->BoundX < 0) ? 0 : Self->BoundX,
      .Right  = Self->BoundX + Self->BoundWidth,
      .Bottom = Self->BoundY + Self->BoundHeight,
      .Top    = (Self->BoundY < 0) ? 0 : Self->BoundY
   };

   Self->VectorClip = clip;

   // Render each event in sequence.

   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      if (e->Blank) continue; // Ignore effects that don't produce graphics

      e->DestX = e->XOffset;
      e->DestY = e->YOffset;

      if (e->InputID) { // This effect has a dependency on another effect.
         if (auto input = find_effect(Self, e->InputID)) { // Offset inheritance from the input
            e->DestX += input->DestX;
            e->DestY += input->DestY;

            input->applyInput(*e); // Applies effect-specific adjustments
         }
      }

      if (auto error = get_banked_bitmap(Self, &e->OutBitmap)) return error;

      // TODO: Clearing of the output bitmap should be a choice left to the effect.
      gfxDrawRectangle(e->OutBitmap, 0, 0, e->OutBitmap->Width, e->OutBitmap->Height, 0x00000000, BAF_FILL);

      e->apply(Self);
   }

   // Render the effect results to the destination bitmap

   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      if ((e->ID) and (e->UsageCount > 0)) { // Don't render effects that are piped to something else.
         log.trace("%s effect is a source for %d clients.", e->Name.c_str(), e->UsageCount);
      }
      else if (auto bmp = e->OutBitmap) { // Not all effects will generate a bitmap
         log.trace("Rendering %s effect %s", e->EffectName.c_str(), e->Name.c_str());
         if (Self->ColourSpace IS CS_LINEAR_RGB) linear2RGB(*bmp);
         if (Self->Opacity < 1.0) bmp->Opacity = 255.0 * Self->Opacity;
         gfxCopyArea(bmp, Self->BkgdBitmap, BAF_BLEND|BAF_COPY, 0, 0, bmp->Width, bmp->Height, 0, 0);
         bmp->Opacity = 255;
      }
      else log.trace("%s effect has no output bitmap.", e->EffectName.c_str());
   }

   return ERR_Okay;
}

//****************************************************************************
// Find an effect by name

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
// Find an effect by UID.

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
         if (auto ref = find_effect(Self, e->InputID)) ref->UsageCount++;
      }

      if (e->MixID) {
         if (auto ref = find_effect(Self, e->MixID)) ref->UsageCount++;
      }
   }
}

//****************************************************************************
// Parser for common filter attributes.

static ERROR fe_default(objVectorFilter *Filter, VectorEffect *Effect, ULONG AttribID, CSTRING Value)
{
   switch (AttribID) {
      case SVF_IN: {
         switch (StrHash(Value, FALSE)) {
            case SVF_SOURCEGRAPHIC:   Effect->SourceType = VSF_GRAPHIC; break;
            case SVF_SOURCEALPHA:     Effect->SourceType = VSF_ALPHA; break;
            case SVF_BACKGROUNDIMAGE: Effect->SourceType = VSF_BKGD; break;
            case SVF_BACKGROUNDALPHA: Effect->SourceType = VSF_BKGD_ALPHA; break;
            case SVF_FILLPAINT:       Effect->SourceType = VSF_FILL; break;
            case SVF_STROKEPAINT:     Effect->SourceType = VSF_STROKE; break;
            default:  {
               if (auto e = find_effect(Filter, Value)) {
                  if (e != Effect) {
                     Effect->SourceType = VSF_REFERENCE;
                     Effect->InputID = e->ID;
                  }
               }
               break;
            }
         }
         break;
      }

      case SVF_IN2: { // 'in2' is the secondary (typically bkgd) that 'in' is copied over or mixed with.
         switch (StrHash(Value, FALSE)) {
            case SVF_SOURCEGRAPHIC:   Effect->MixType = VSF_GRAPHIC; break;
            case SVF_SOURCEALPHA:     Effect->MixType = VSF_ALPHA; break;
            case SVF_BACKGROUNDIMAGE: Effect->MixType = VSF_BKGD; break;
            case SVF_BACKGROUNDALPHA: Effect->MixType = VSF_BKGD_ALPHA; break;
            case SVF_FILLPAINT:       Effect->MixType = VSF_FILL; break;
            case SVF_STROKEPAINT:     Effect->MixType = VSF_STROKE; break;
            default:  {
               if (auto e = find_effect(Filter, Value)) {
                  if (e != Effect) {
                     Effect->MixType  = VSF_REFERENCE;
                     Effect->MixID = e->ID;
                  }
               }
               break;
            }
         }
         break;
      }

      case SVF_RESULT: // Names the filter.  This allows another filter to use the result as 'in' and create a pipeline
         Effect->Name = Value;
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

   Self->BankIndex = 0;

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

Multiple calls to this action will append to existing effects.  If a reset of existing effects is necessary, call the
@Clear action before appending new effects.

-END-

*****************************************************************************/

static ERROR VECTORFILTER_DataFeed(objVectorFilter *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   ERROR error = ERR_Okay;
   if (Args->DataType IS DATA_XML) {
      objXML *xml;
      if (!CreateObject(ID_XML, NF_INTEGRAL, &xml,
            FID_Statement, (CSTRING)Args->Buffer,
            TAGEND)) {
         for (auto tag = xml->Tags[0]; tag; tag=tag->Next) {
            log.trace("Parsing filter element '%s'", tag->Attrib->Name);
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
         }

         calc_usage(Self);
         acFree(xml);
      }
      else error = ERR_CreateObject;
   }

   return error;
}

//****************************************************************************

static ERROR VECTORFILTER_Free(objVectorFilter *Self, APTR Void)
{
   VECTORFILTER_Clear(Self, NULL); // Free cache allocations

   Self->Effects.~vector();

   if (Self->SourceGraphic) { acFree(Self->SourceGraphic); Self->SourceGraphic = NULL; }
   if (Self->Scene)         { acFree(Self->Scene);         Self->Scene = NULL; }
   if (Self->Path)          { FreeResource(Self->Path);    Self->Path = NULL; }
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

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORFILTER_NewObject(objVectorFilter *Self, APTR Void)
{
   new (&Self->Effects) std::vector<VectorEffect>;
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
EffectXML: Returns a SVG XML string that defines the filter's effects.

This field value will return a purpose-built string that defines the filter's effects in SVG compliant XML.  The string
is allocated and must be freed once no longer in use.

-END-

*****************************************************************************/

static ERROR VECTORFILTER_GET_EffectXML(objVectorFilter *Self, CSTRING *Value)
{
   std::stringstream ss;

   for (auto &ptr : Self->Effects) {
      auto e = ptr.get();
      ss << "<";
      e->xml(ss);
      ss << "/>";
   }

   auto str = ss.str();
   if ((*Value = StrClone(str.c_str()))) return ERR_Okay;
   else return ERR_AllocMemory;
}

/*****************************************************************************

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
Units: Defines the coordinate system for fields X, Y, Width and Height.

The default coordinate system for gradients is `BOUNDING_BOX`, which positions the filter around the vector that
references it.  The alternative is `USERSPACE`, which positions the filter relative to the current viewport.

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
   { "Inherit",        FDF_OBJECT|FDF_RW,           0, NULL, (APTR)VECTORFILTER_SET_Inherit },
   { "Units",          FDF_LONG|FDF_LOOKUP|FDF_RW,  (MAXINT)&clVectorFilterUnits, NULL, NULL },
   { "PrimitiveUnits", FDF_LONG|FDF_LOOKUP|FDF_RW,  (MAXINT)&clVectorFilterPrimitiveUnits, NULL, NULL },
   { "Dimensions",     FDF_LONGFLAGS|FDF_R,         (MAXINT)&clFilterDimensions, NULL, NULL },
   { "ColourSpace",    FDF_LONG|FDF_LOOKUP|FDF_RW,  (MAXINT)&clVectorFilterColourSpace, NULL, NULL },
   // Virtual fields
   { "EffectXML",      FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)VECTORFILTER_GET_EffectXML, NULL },
   { "Path",           FDF_VIRTUAL|FDF_STRING|FDF_W, 0, NULL, (APTR)VECTORFILTER_SET_Path },
   END_FIELD
};

ERROR init_filter(void)
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

