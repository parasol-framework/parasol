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
   objVector *Source;     // The vector branch to render as source graphic.
   objVectorScene *Scene;
   LONG AspectRatio;      // Aspect ratio flags.
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
   auto target = calc_target_area(Self);

   // The configuration of the img values must be identical to the ImageFX code.

   DOUBLE img_x = target.x;
   DOUBLE img_y = target.y;
   DOUBLE img_width = target.width;
   DOUBLE img_height = target.height;

   if (filter->PrimitiveUnits IS VUNIT_BOUNDING_BOX) {
      if (Self->Dimensions & (DMF_FIXED_X|DMF_RELATIVE_X)) img_x = trunc(target.x + (Self->X * target.bound_width));
      if (Self->Dimensions & (DMF_FIXED_Y|DMF_RELATIVE_Y)) img_y = trunc(target.y + (Self->Y * target.bound_height));
      if (Self->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH)) img_width = Self->Width * target.bound_width;
      if (Self->Dimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT)) img_height = Self->Height * target.bound_height;
   }
   else {
      if (Self->Dimensions & DMF_RELATIVE_X)   img_x = target.x + (Self->X * target.width);
      else if (Self->Dimensions & DMF_FIXED_X) img_x = Self->X;

      if (Self->Dimensions & DMF_RELATIVE_Y)   img_y = target.y + (Self->Y * target.height);
      else if (Self->Dimensions & DMF_FIXED_Y) img_y = Self->Y;

      if (Self->Dimensions & DMF_RELATIVE_WIDTH)   img_width = target.width * Self->Width;
      else if (Self->Dimensions & DMF_FIXED_WIDTH) img_width = Self->Width;

      if (Self->Dimensions & DMF_RELATIVE_HEIGHT)   img_height = target.height * Self->Height;
      else if (Self->Dimensions & DMF_FIXED_HEIGHT) img_height = Self->Height;
   }

   LONG cs = (filter->ColourSpace IS VCS_LINEAR_RGB) ? VCS_LINEAR_RGB : VCS_SRGB;
   if (Self->Scene->Viewport->ColourSpace != cs) {
      SetLong(Self->Scene->Viewport, FID_ColourSpace, cs);
   }

   if ((target.width > Self->Scene->PageWidth) or (target.height > Self->Scene->PageHeight)) {
      acResize(Self->Scene, filter->ClientViewport->Scene->PageWidth, filter->ClientViewport->Scene->PageHeight, 0);
   }

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

   // This rendering implementation is done in run-time.  Allocating a bitmap cache would
   // be a more optimal alternative.

   ((extVectorViewport *)Self->Scene->Viewport)->Matrices = &matrix;
   Self->Scene->Viewport->Child = Self->Source;
   auto save_parent = Self->Source->Parent;
   Self->Source->Parent = Self->Scene->Viewport;

   auto const save_next = Self->Source->Next; // Switch off the Next pointer to prevent processing of siblings.
   Self->Source->Next = NULL;
   filter->Disabled = true; // Turning off the filter is required to prevent infinite recursion.

   mark_dirty(Self->Scene->Viewport, RC_TRANSFORM);

   Self->Scene->Bitmap = Self->Target;
   acDraw(Self->Scene);

   filter->Disabled = false;
   Self->Source->Parent = save_parent;
   Self->Source->Next = save_next;
   ((extVectorViewport *)Self->Scene->Viewport)->Matrices = NULL;
   mark_dirty(Self->Source, RC_ALL);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_Free(objSourceFX *Self, APTR Void)
{
   if (Self->Source) { UnsubscribeAction(Self->Source, AC_Free); Self->Source = NULL; }
   if (Self->Scene)  { acFree(Self->Scene); Self->Scene = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_Init(objSourceFX *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->Source) return log.warning(ERR_UndefinedField);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SOURCEFX_NewObject(objSourceFX *Self, APTR Void)
{
   Self->AspectRatio = ARF_X_MID|ARF_Y_MID|ARF_MEET;
   Self->SourceType  = VSF_NONE;

   if (!CreateObject(ID_VECTORSCENE, NF_INTEGRAL, &Self->Scene,
         FID_Name|TSTR,        "fx_src_scene",
         FID_PageWidth|TLONG,  1,
         FID_PageHeight|TLONG, 1,
         TAGEND)) {

      objVectorViewport *vp;
      if (!CreateObject(ID_VECTORVIEWPORT, 0, &vp,
            FID_Name|TSTR,         "fx_src_viewport",
            FID_Owner|TLONG,       Self->Scene->UID,
            FID_ColourSpace|TLONG, VCS_LINEAR_RGB,
            TAGEND)) {
      }
      else return ERR_CreateObject;
   }
   else return ERR_CreateObject;

   return ERR_Okay;
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
