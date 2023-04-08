/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
SourceFX: Renders a source vector in the effect pipeline.

The SourceFX class will render a named vector into a given rectangle within the current user coordinate system.

Technically the SourceFX object is represented by a new viewport, the bounds of which are defined by attributes X, Y,
Width and Height.  The placement and scaling of the referenced vector is controlled by the #AspectRatio field.

-END-

NOTE: This class exists to meet the needs of the SVG feImage element in a specific case where the href refers to a
registered vector rather than an image file.

*********************************************************************************************************************/

class extSourceFX : public extFilterEffect {
   public:
   static constexpr CLASSID CLASS_ID = ID_SOURCEFX;
   static constexpr CSTRING CLASS_NAME = "SourceFX";
   using create = pf::Create<extSourceFX>;

   objBitmap *Bitmap;     // Rendered image cache.
   objVector *Source;     // The vector branch to render as source graphic.
   objVectorScene *Scene; // Internal scene for rendering.
   UBYTE *BitmapData;
   LONG AspectRatio;      // Aspect ratio flags.
   LONG DataSize;
   bool Render;           // Must be true if the bitmap cache needs to be rendered.
};

//********************************************************************************************************************

static void notify_free_source(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   ((extSourceFX *)CurrentContext())->Source = NULL;
}

/*********************************************************************************************************************
-ACTION-
Draw: Render the source vector to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR SOURCEFX_Draw(extSourceFX *Self, struct acDraw *Args)
{
   pf::Log log;

   if (!Self->Source) return ERR_Okay;

   auto &filter = Self->Filter;

   // The configuration of the img values must be identical to the ImageFX code.

   DOUBLE img_x = filter->TargetX;
   DOUBLE img_y = filter->TargetY;
   DOUBLE img_width = filter->TargetWidth;
   DOUBLE img_height = filter->TargetHeight;

   if (filter->PrimitiveUnits IS VUNIT_BOUNDING_BOX) {
      if (Self->Dimensions & (DMF_FIXED_X|DMF_RELATIVE_X)) img_x = trunc(filter->TargetX + (Self->X * filter->BoundWidth));
      if (Self->Dimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y)) img_y = trunc(filter->TargetY + (Self->Y * filter->BoundHeight));
      if (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH)) img_width = Self->Width * filter->BoundWidth;
      if (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT)) img_height = Self->Height * filter->BoundHeight;
   }
   else {
      if (Self->Dimensions & DMF_RELATIVE_X)   img_x = filter->TargetX + (Self->X * filter->TargetWidth);
      else if (Self->Dimensions & DMF_FIXED_X) img_x = Self->X;

      if (Self->Dimensions & DMF_RELATIVE_Y)   img_y = filter->TargetY + (Self->Y * filter->TargetHeight);
      else if (Self->Dimensions & DMF_FIXED_Y) img_y = Self->Y;

      if (Self->Dimensions & DMF_RELATIVE_WIDTH)   img_width = filter->TargetWidth * Self->Width;
      else if (Self->Dimensions & DMF_FIXED_WIDTH) img_width = Self->Width;

      if (Self->Dimensions & DMF_RELATIVE_HEIGHT)   img_height = filter->TargetHeight * Self->Height;
      else if (Self->Dimensions & DMF_FIXED_HEIGHT) img_height = Self->Height;
   }

   if ((filter->ClientViewport->Scene->PageWidth > Self->Scene->PageWidth) or
       (filter->ClientViewport->Scene->PageHeight > Self->Scene->PageHeight)) {
      acResize(Self->Scene, filter->ClientViewport->Scene->PageWidth, filter->ClientViewport->Scene->PageHeight, 0);
   }

   if ((filter->VectorClip.Right > Self->Bitmap->Clip.Right) or
       (filter->VectorClip.Bottom > Self->Bitmap->Clip.Bottom)) {
      acResize(Self->Bitmap, filter->ClientViewport->Scene->PageWidth, filter->ClientViewport->Scene->PageHeight, 0);
   }

   auto vp = (extVectorViewport *)Self->Scene->Viewport;
   if ((img_x != vp->vpViewX) or (img_y != vp->vpViewY) or (img_width != vp->vpViewWidth) or (img_height != vp->vpViewHeight)) {
      Self->Render = true;
   }

   if (Self->Render) {
      auto &cache = Self->Bitmap;
      cache->Clip = filter->VectorClip;

      // Manual data management - bitmap data is restricted to the clipping region.

      const LONG canvas_width  = cache->Clip.Right - cache->Clip.Left;
      const LONG canvas_height = cache->Clip.Bottom - cache->Clip.Top;
      cache->LineWidth = canvas_width * cache->BytesPerPixel;

      if ((Self->BitmapData) and (Self->DataSize < cache->LineWidth * canvas_height)) {
         FreeResource(Self->BitmapData);
         Self->BitmapData = NULL;
         cache->Data = NULL;
      }

      if (!cache->Data) {
         if (!AllocMemory(cache->LineWidth * canvas_height, MEM_DATA|MEM_NO_CLEAR, &Self->BitmapData)) {
            Self->DataSize = cache->LineWidth * canvas_height;
         }
         else return ERR_AllocMemory;
      }

      cache->Data = Self->BitmapData - (cache->Clip.Left * cache->BytesPerPixel) - (cache->Clip.Top * cache->LineWidth);

      Self->Scene->Viewport->setFields(fl::X(img_x), fl::Y(img_y), fl::Width(img_width), fl::Height(img_height),
         fl::AspectRatio(Self->AspectRatio));

      auto &t = filter->ClientVector->Transform;
      VectorMatrix matrix = {
         .Next = NULL, .Vector = Self->Scene->Viewport,
         .ScaleX = t.sx, .ShearY = t.shy, .ShearX = t.shx, .ScaleY = t.sy, .TranslateX = t.tx, .TranslateY = t.ty
      };

      ((extVectorViewport *)Self->Scene->Viewport)->Matrices = &matrix;

      auto save_parent = Self->Source->Parent;
      auto const save_next = Self->Source->Next;
      Self->Scene->Viewport->Child = Self->Source;
      Self->Source->Parent = Self->Scene->Viewport;
      Self->Source->Next = NULL;

      filter->Disabled = true; // Turning off the filter is required to prevent infinite recursion.

      mark_dirty(Self->Scene->Viewport, RC_TRANSFORM);

      Self->Scene->Bitmap = cache;
      gfxDrawRectangle(cache, 0, 0, cache->Width, cache->Height, 0x00000000, BAF_FILL);
      acDraw(Self->Scene);

      filter->Disabled = false;
      Self->Scene->Viewport->Child = NULL;
      Self->Source->Parent = save_parent;
      Self->Source->Next   = save_next;
      ((extVectorViewport *)Self->Scene->Viewport)->Matrices = NULL;
      mark_dirty(Self->Source, RC_ALL);
   }

   gfxCopyArea(Self->Bitmap, Self->Target, 0, 0, 0, Self->Bitmap->Width, Self->Bitmap->Height, 0, 0);

   Self->Render = false;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_Free(extSourceFX *Self, APTR Void)
{
   if (Self->Bitmap)     { FreeResource(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->Source)     { UnsubscribeAction(Self->Source, AC_Free); Self->Source = NULL; }
   if (Self->Scene)      { FreeResource(Self->Scene); Self->Scene = NULL; }
   if (Self->BitmapData) { FreeResource(Self->BitmapData); Self->BitmapData = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_Init(extSourceFX *Self, APTR Void)
{
   pf::Log log;

   if (!Self->Source) return log.warning(ERR_UndefinedField);

   Self->Scene->Viewport->setColourSpace(Self->Filter->ColourSpace);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_NewObject(extSourceFX *Self, APTR Void)
{
   Self->AspectRatio = ARF_X_MID|ARF_Y_MID|ARF_MEET;
   Self->SourceType  = VSF_NONE;
   Self->Render      = true;

   if ((Self->Scene = objVectorScene::create::integral(fl::Name("fx_src_scene"), fl::PageWidth(1), fl::PageHeight(1)))) {
      if (objVectorViewport::create::global(fl::Name("fx_src_viewport"), fl::Owner(Self->Scene->UID))) {
         if ((Self->Bitmap = objBitmap::create::integral(fl::Name("fx_src_cache"),
               fl::Width(1),
               fl::Height(1),
               fl::BitsPerPixel(32),
               fl::Flags(BMF_ALPHA_CHANNEL|BMF_NO_DATA)))) {
            return ERR_Okay;
         }
         else return ERR_CreateObject;
      }
      else return ERR_CreateObject;
   }
   else return ERR_CreateObject;
}

/*********************************************************************************************************************

-FIELD-
AspectRatio: SVG compliant aspect ratio settings.
Lookup: ARF

*********************************************************************************************************************/

static ERROR SOURCEFX_GET_AspectRatio(extSourceFX *Self, LONG *Value)
{
   *Value = Self->AspectRatio;
   return ERR_Okay;
}

static ERROR SOURCEFX_SET_AspectRatio(extSourceFX *Self, LONG Value)
{
   Self->AspectRatio = Value;
   Self->Render = true;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Source: The source @Vector that will be rendered.

The Source field must refer to a @Vector that will be rendered in the filter pipeline.  The vector must be under the
ownership of the same @VectorScene that the filter pipeline belongs.

*********************************************************************************************************************/

static ERROR SOURCEFX_SET_Source(extSourceFX *Self, objVector *Value)
{
   pf::Log log;
   if (!Value) return log.warning(ERR_InvalidValue);
   if (Value->ClassID != ID_VECTOR) return log.warning(ERR_WrongClass);

   if (Self->Source) UnsubscribeAction(Self->Source, AC_Free);
   Self->Source = Value;
   auto callback = make_function_stdc(notify_free_source);
   SubscribeAction(Value, AC_Free, &callback);
   Self->Render = true;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
SourceName: Name of a source definition to be rendered.

Setting Def to the name of a pre-registered scene definition will reference that object in #Source.  If the name is
not registered then `ERR_Search` is returned.  The named object must be derived from the @Vector class.

Vectors are registered via the @VectorScene AddDef() method.

*********************************************************************************************************************/

static ERROR SOURCEFX_SET_SourceName(extSourceFX *Self, CSTRING Value)
{
   pf::Log log;

   if ((!Self->Filter) or (!Self->Filter->Scene)) log.warning(ERR_UndefinedField);

   if (Self->Source) {
      UnsubscribeAction(Self->Source, AC_Free);
      Self->Source = NULL;
   }

   objVector *src;
   if (!scFindDef(Self->Filter->Scene, Value, (OBJECTPTR *)&src)) {
      if (src->ClassID != ID_VECTOR) return log.warning(ERR_WrongClass);
      Self->Source = src;
      auto callback = make_function_stdc(notify_free_source);
      SubscribeAction(src, AC_Free, &callback);
      Self->Render = true;
      return ERR_Okay;
   }
   else return log.warning(ERR_Search);
}

/*********************************************************************************************************************

-FIELD-
XMLDef: Returns an SVG compliant XML string that describes the filter.
-END-

*********************************************************************************************************************/

static ERROR SOURCEFX_GET_XMLDef(extSourceFX *Self, STRING *Value)
{
   *Value = StrClone("feImage");
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_source_def.c"

static const FieldArray clSourceFXFields[] = {
   { "AspectRatio", FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, SOURCEFX_GET_AspectRatio, SOURCEFX_SET_AspectRatio, &clAspectRatio },
   { "SourceName",  FDF_VIRTUAL|FDF_STRING|FDF_I, NULL, SOURCEFX_SET_SourceName },
   { "Source",      FDF_VIRTUAL|FDF_OBJECT|FDF_R, NULL, SOURCEFX_SET_Source, ID_VECTOR },
   { "XMLDef",      FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, SOURCEFX_GET_XMLDef },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_sourcefx(void)
{
   clSourceFX = objMetaClass::create::global(
      fl::BaseClassID(ID_FILTEREFFECT),
      fl::ClassID(ID_SOURCEFX),
      fl::Name("SourceFX"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clSourceFXActions),
      fl::Fields(clSourceFXFields),
      fl::Size(sizeof(extSourceFX)),
      fl::Path(MOD_PATH));

   return clSourceFX ? ERR_Okay : ERR_AddClass;
}
