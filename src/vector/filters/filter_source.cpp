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

class objSourceFX : public extFilterEffect {
   public:
   objBitmap *Bitmap;     // Rendered image cache.
   objVector *Source;     // The vector branch to render as source graphic.
   objVectorScene *Scene; // Internal scene for rendering.
   UBYTE *BitmapData;
   LONG AspectRatio;      // Aspect ratio flags.
   LONG DataSize;
   bool Render;           // Must be true if the bitmap cache needs to be rendered.
};

//********************************************************************************************************************

static ERROR SOURCEFX_ActionNotify(objSourceFX *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) Self->Source = NULL;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Draw: Render the source vector to the target bitmap.
-END-
*********************************************************************************************************************/

static ERROR SOURCEFX_Draw(objSourceFX *Self, struct acDraw *Args)
{
   parasol::Log log;

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
         if (!AllocMemory(cache->LineWidth * canvas_height, MEM_DATA|MEM_NO_CLEAR, &Self->BitmapData, NULL)) {
            Self->DataSize = cache->LineWidth * canvas_height;
         }
         else return ERR_AllocMemory;
      }

      cache->Data = Self->BitmapData - (cache->Clip.Left * cache->BytesPerPixel) - (cache->Clip.Top * cache->LineWidth);

      SetFields(Self->Scene->Viewport,
         FID_X|TDOUBLE,         img_x,
         FID_Y|TDOUBLE,         img_y,
         FID_Width|TDOUBLE,     img_width,
         FID_Height|TDOUBLE,    img_height,
         FID_AspectRatio|TLONG, Self->AspectRatio,
         TAGEND);

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

static ERROR SOURCEFX_Free(objSourceFX *Self, APTR Void)
{
   if (Self->Bitmap)     { acFree(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->Source)     { UnsubscribeAction(Self->Source, AC_Free); Self->Source = NULL; }
   if (Self->Scene)      { acFree(Self->Scene); Self->Scene = NULL; }
   if (Self->BitmapData) { FreeResource(Self->BitmapData); Self->BitmapData = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_Init(objSourceFX *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->Source) return log.warning(ERR_UndefinedField);

   Self->Scene->Viewport->set(FID_ColourSpace, Self->Filter->ColourSpace);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_NewObject(objSourceFX *Self, APTR Void)
{
   Self->AspectRatio = ARF_X_MID|ARF_Y_MID|ARF_MEET;
   Self->SourceType  = VSF_NONE;
   Self->Render      = true;

   if (!CreateObject(ID_VECTORSCENE, NF_INTEGRAL, &Self->Scene,
         FID_Name|TSTR,        "fx_src_scene",
         FID_PageWidth|TLONG,  1,
         FID_PageHeight|TLONG, 1,
         TAGEND)) {

      objVectorViewport *vp;
      if (!CreateObject(ID_VECTORVIEWPORT, 0, &vp,
            FID_Name|TSTR,         "fx_src_viewport",
            FID_Owner|TLONG,       Self->Scene->UID,
            TAGEND)) {

         if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->Bitmap,
               FID_Name|TSTR,          "fx_src_cache",
               FID_Width|TLONG,        1,
               FID_Height|TLONG,       1,
               FID_BitsPerPixel|TLONG, 32,
               FID_Flags|TLONG,        BMF_ALPHA_CHANNEL|BMF_NO_DATA,
               TAGEND)) {
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

static ERROR SOURCEFX_GET_AspectRatio(objSourceFX *Self, LONG *Value)
{
   *Value = Self->AspectRatio;
   return ERR_Okay;
}

static ERROR SOURCEFX_SET_AspectRatio(objSourceFX *Self, LONG Value)
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

static ERROR SOURCEFX_SET_Source(objSourceFX *Self, objVector *Value)
{
   parasol::Log log;
   if (!Value) return log.warning(ERR_InvalidValue);
   if (Value->ClassID != ID_VECTOR) return log.warning(ERR_WrongClass);

   if (Self->Source) UnsubscribeAction(Self->Source, AC_Free);
   Self->Source = Value;
   SubscribeAction(Value, AC_Free);
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

static ERROR SOURCEFX_SET_SourceName(objSourceFX *Self, CSTRING Value)
{
   parasol::Log log;

   if ((!Self->Filter) or (!Self->Filter->Scene)) log.warning(ERR_UndefinedField);

   if (Self->Source) {
      UnsubscribeAction(Self->Source, AC_Free);
      Self->Source = NULL;
   }

   objVector *src;
   if (!scFindDef(Self->Filter->Scene, Value, (OBJECTPTR *)&src)) {
      if (src->ClassID != ID_VECTOR) return log.warning(ERR_WrongClass);
      Self->Source = src;
      SubscribeAction(src, AC_Free);
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

static ERROR SOURCEFX_GET_XMLDef(objSourceFX *Self, STRING *Value)
{
   *Value = StrClone("feImage");
   return ERR_Okay;
}

//********************************************************************************************************************

#include "filter_source_def.c"

static const FieldArray clSourceFXFields[] = {
   { "AspectRatio",    FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clAspectRatio, (APTR)SOURCEFX_GET_AspectRatio, (APTR)SOURCEFX_SET_AspectRatio },
   { "SourceName",     FDF_VIRTUAL|FDF_STRING|FDF_I,           0, NULL, (APTR)SOURCEFX_SET_SourceName },
   { "Source",         FDF_VIRTUAL|FDF_OBJECT|FDF_R,           ID_VECTOR, NULL, (APTR)SOURCEFX_SET_Source },
   { "XMLDef",         FDF_VIRTUAL|FDF_STRING|FDF_ALLOC|FDF_R, 0, (APTR)SOURCEFX_GET_XMLDef, NULL },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_sourcefx(void)
{
   return(CreateObject(ID_METACLASS, 0, &clSourceFX,
      FID_BaseClassID|TLONG, ID_FILTEREFFECT,
      FID_SubClassID|TLONG,  ID_SOURCEFX,
      FID_Name|TSTRING,      "SourceFX",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,      clSourceFXActions,
      FID_Fields|TARRAY,     clSourceFXFields,
      FID_Size|TLONG,        sizeof(objSourceFX),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
