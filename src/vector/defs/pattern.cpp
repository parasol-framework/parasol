/*****************************************************************************

Please note that this is not an extension of the Vector class.  It is used for the purposes of pattern definitions only.

-CLASS-
VectorPattern: Provides support for the filling and stroking of vectors with patterns.

The VectorPattern class is used by Vector painting algorithms to fill and stroke vectors with pre-rendered patterns.
It is the most efficient way of rendering a common set of graphics multiple times.

The VectorPattern must be registered with a @VectorScene via the <method class="VectorScene">AddDef</> method.
Any vector within the target scene will be able to utilise the pattern for filling or stroking by referencing its
name through the @Vector.Fill and @Vector.Stroke fields.  For instance 'url(#dots)'.

A special use case is made for patterns that are applied as a fill operation in @VectorViewport objects.  In this
case the renderer will dynamically render the pattern as a background within the viewport.  This ensures that the
pattern is rendered at maximum fidelity whenever it is used, and not affected by bitmap clipping restrictions.  It
should be noted that this means the image caching feature will be disabled.

It is strongly recommended that the VectorPattern is owned by the @VectorScene that is handling the
definition.  This will ensure that the VectorPattern is deallocated when the scene is destroyed.
-END-

NOTE: The VectorPattern inherits attributes from the VectorScene, which is used to define the size of the pattern and
contains the pattern content.

*****************************************************************************/

static ERROR PATTERN_Draw(extVectorPattern *Self, struct acDraw *Args)
{
   parasol::Log log;

   if (!Self->Scene->PageWidth) return log.warning(ERR_FieldNotSet);
   if (!Self->Scene->PageHeight) return log.warning(ERR_FieldNotSet);

   if (Self->Bitmap) {
      if ((Self->Scene->PageWidth != Self->Bitmap->Width) or (Self->Scene->PageHeight != Self->Bitmap->Height)) {
         acResize(Self->Bitmap, Self->Scene->PageWidth, Self->Scene->PageHeight, 32);
      }
   }
   else if (!(Self->Bitmap = objBitmap::create::integral(
      fl::Width(Self->Scene->PageWidth),
      fl::Height(Self->Scene->PageHeight),
      fl::Flags(BMF_ALPHA_CHANNEL),
      fl::BitsPerPixel(32)))) return ERR_CreateObject;

   ClearMemory(Self->Bitmap->Data, Self->Bitmap->LineWidth * Self->Bitmap->Height);
   Self->Scene->Bitmap = Self->Bitmap;
   acDraw(Self->Scene);

   return ERR_Okay;
}

//****************************************************************************

static ERROR PATTERN_Free(extVectorPattern *Self, APTR Void)
{
   VectorMatrix *next;
   for (auto scan=Self->Matrices; scan; scan=next) {
      next = scan->Next;
      FreeResource(scan);
   }
   Self->Matrices = NULL;

   if (Self->Bitmap) { acFree(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->Scene)  { acFree(Self->Scene); Self->Scene = NULL; }

   return ERR_Okay;
}

//****************************************************************************

static ERROR PATTERN_Init(extVectorPattern *Self, APTR Void)
{
   parasol::Log log;

   if ((Self->SpreadMethod <= 0) or (Self->SpreadMethod >= VSPREAD_END)) {
      log.traceWarning("Invalid SpreadMethod value of %d", Self->SpreadMethod);
      return log.warning(ERR_OutOfRange);
   }

   if ((Self->Units <= 0) or (Self->Units >= VUNIT_END)) {
      log.traceWarning("Invalid Units value of %d", Self->Units);
      return log.warning(ERR_OutOfRange);
   }

   if (!Self->Width) {
      Self->Width = 1;
      Self->Dimensions |= DMF_FIXED_WIDTH;
   }

   if (!Self->Height) {
      Self->Height = 1;
      Self->Dimensions |= DMF_FIXED_HEIGHT;
   }

   if (acInit(Self->Scene)) return ERR_Init;
   if (acInit(Self->Viewport)) return ERR_Init;

   return ERR_Okay;
}

//****************************************************************************

static ERROR PATTERN_NewObject(extVectorPattern *Self, APTR Void)
{
   if (!NewObject(ID_VECTORSCENE, NF::INTEGRAL, &Self->Scene)) {
      if (!NewObject(ID_VECTORVIEWPORT, &Self->Viewport)) {
         SetOwner(Self->Viewport, Self->Scene);
         Self->SpreadMethod = VSPREAD_REPEAT;
         Self->Units        = VUNIT_BOUNDING_BOX;
         Self->ContentUnits = VUNIT_USERSPACE;
         Self->Opacity      = 1.0;
         return ERR_Okay;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************

-FIELD-
ContentUnits: Private. Not yet implemented.

In compliance with SVG requirements, the application of ContentUnits is only effective if the Viewport's X, Y, Width
and Height fields have been defined.  The default setting is USERSPACE.

-FIELD-
Dimensions: Dimension flags are stored here.

-FIELD-
Height: Height of the pattern tile.

The (Width,Height) field values define the dimensions of the pattern tile.  If the provided value is a percentage
then the dimension is calculated relative to the bounding box or viewport applying the pattern, dependent on the
#Units setting.

*****************************************************************************/

static ERROR PATTERN_GET_Height(extVectorPattern *Self, Variable *Value)
{
   DOUBLE val = Self->Height;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_HEIGHT)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR PATTERN_SET_Height(extVectorPattern *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_HEIGHT) & (~DMF_FIXED_HEIGHT);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);

   Self->Height = val;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Inherit: Inherit attributes from a VectorPattern referenced here.

Attributes can be inherited from another pattern by referencing it in this field.  This feature is provided
primarily for the purpose of simplifying SVG compatibility and its use may result in an unnecessary performance
penalty.

*****************************************************************************/

static ERROR PATTERN_SET_Inherit(extVectorPattern *Self, extVectorPattern *Value)
{
   if (Value) {
      if (Value->ClassID IS ID_VECTORPATTERN) {
         Self->Inherit = Value;
      }
      else return ERR_InvalidValue;
   }
   else Self->Inherit = NULL;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Matrices: A linked list of transform matrices that have been applied to the pattern.

All transforms that have been applied to the pattern can be read from the Matrices field.  Each transform is
represented by a VectorMatrix structure, and are linked in the order in which they were applied to the pattern.

&VectorMatrix

*****************************************************************************/

static ERROR VECTORPATTERN_GET_Matrices(extVectorPattern *Self, VectorMatrix **Value)
{
   *Value = Self->Matrices;
   return ERR_Okay;
}

static ERROR VECTORPATTERN_SET_Matrices(extVectorPattern *Self, VectorMatrix *Value)
{
   if (!Value) {
      auto hook = &Self->Matrices;
      while (Value) {
         VectorMatrix *matrix;
         if (!AllocMemory(sizeof(VectorMatrix), MEM_DATA|MEM_NO_CLEAR, &matrix, NULL)) {
            matrix->Vector = NULL;
            matrix->Next   = NULL;
            matrix->ScaleX = Value->ScaleX;
            matrix->ScaleY = Value->ScaleY;
            matrix->ShearX = Value->ShearX;
            matrix->ShearY = Value->ShearY;
            matrix->TranslateX = Value->TranslateX;
            matrix->TranslateY = Value->TranslateY;
            *hook = matrix;
            hook = &matrix->Next;
         }
         else return ERR_AllocMemory;

         Value = Value->Next;
      }
   }
   else {
      VectorMatrix *next;
      for (auto scan=Self->Matrices; scan; scan=next) {
         next = scan->Next;
         FreeResource(scan);
      }
      Self->Matrices = NULL;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Opacity: The opacity of the pattern.

The opacity of the pattern is defined as a value between 0.0 and 1.0, with 1.0 being fully opaque.  The default value
is 1.0.

*****************************************************************************/

static ERROR PATTERN_SET_Opacity(extVectorPattern *Self, DOUBLE Value)
{
   if (Value < 0.0) Value = 0;
   else if (Value > 1.0) Value = 1.0;
   Self->Opacity = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Scene: Refers to the internal @VectorScene that will contain the rendered pattern.

The VectorPattern class allocates a @VectorScene in this field and inherits its functionality.  In addition,
a @VectorViewport class will be assigned to the scene and is referenced in the #Viewport field for
managing the vectors that will be rendered.

-FIELD-
SpreadMethod: The behaviour to use when the pattern bounds do not match the vector path.

Indicates what happens if the pattern starts or ends inside the bounds of the target vector.  The default value is PAD.

-FIELD-
Transform: Applies a transform to the pattern during the render process.

A transform can be applied to the pattern by setting this field with an SVG compliant transform string.

*****************************************************************************/

static ERROR PATTERN_SET_Transform(extVectorPattern *Self, CSTRING Commands)
{
   parasol::Log log;

   if (!Commands) return log.warning(ERR_InvalidValue);

   if (!Self->Matrices) {
      VectorMatrix *matrix;
      if (!AllocMemory(sizeof(VectorMatrix), MEM_DATA|MEM_NO_CLEAR, &matrix, NULL)) {
         matrix->Vector = NULL;
         matrix->Next   = Self->Matrices;
         matrix->ScaleX = 1.0;
         matrix->ScaleY = 1.0;
         matrix->ShearX = 0;
         matrix->ShearY = 0;
         matrix->TranslateX = 0;
         matrix->TranslateY = 0;

         Self->Matrices = matrix;
         return vecParseTransform(Self->Matrices, Commands);
      }
      else return ERR_AllocMemory;
   }
   else {
      vecResetMatrix(Self->Matrices);
      return vecParseTransform(Self->Matrices, Commands);
   }
}

/*****************************************************************************

-FIELD-
Units:  Defines the coordinate system for fields X, Y, Width and Height.

This field declares the coordinate system that is used for values in the #X and #Y fields.  The default setting is
`BOUNDING_BOX`, which means the pattern will be drawn to scale in realtime.  The most efficient method is USERSPACE,
which allows the pattern image to be persistently cached.

-FIELD-
Viewport: Refers to the viewport that contains the pattern.

The Viewport refers to a @VectorViewport object that is created to host the vectors for the rendered pattern.  If the
Viewport does not contain at least one vector that renders an image, the pattern will be ineffective.

*****************************************************************************/

static ERROR PATTERN_GET_Viewport(extVectorPattern *Self, extVectorViewport **Value)
{
   *Value = Self->Viewport;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Width: Width of the pattern tile.

The (Width,Height) field values define the dimensions of the pattern tile.  If the provided value is a percentage
then the dimension is calculated relative to the bounding box or viewport applying the pattern, dependent on the
#Units setting.

*****************************************************************************/

static ERROR PATTERN_GET_Width(extVectorPattern *Self, Variable *Value)
{
   DOUBLE val = Self->Width;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_WIDTH)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR PATTERN_SET_Width(extVectorPattern *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_WIDTH) & (~DMF_FIXED_WIDTH);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);

   Self->Width = val;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
X: X coordinate for the pattern.

The (X,Y) field values define the starting coordinate for mapping patterns.

*****************************************************************************/

static ERROR PATTERN_GET_X(extVectorPattern *Self, Variable *Value)
{
   DOUBLE val = Self->X;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR PATTERN_SET_X(extVectorPattern *Self, Variable *Value)
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
Y: Y coordinate for the pattern.

The (X,Y) field values define the starting coordinate for mapping patterns.
-END-

*****************************************************************************/

static ERROR PATTERN_GET_Y(extVectorPattern *Self, Variable *Value)
{
   DOUBLE val = Self->Y;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR PATTERN_SET_Y(extVectorPattern *Self, Variable *Value)
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

/****************************************************************************/

static const ActionArray clPatternActions[] = {
   { AC_Draw,      (APTR)PATTERN_Draw },
   { AC_Free,      (APTR)PATTERN_Free },
   { AC_Init,      (APTR)PATTERN_Init },
   { AC_NewObject, (APTR)PATTERN_NewObject },
   { 0, NULL }
};

static const FieldDef clPatternDimensions[] = {
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

static const FieldDef clPatternUnits[] = {
   { "BoundingBox", VUNIT_BOUNDING_BOX },  // Coordinates are relative to the object's bounding box
   { "UserSpace",   VUNIT_USERSPACE },    // Coordinates are relative to the current viewport
   { NULL, 0 }
};

static const FieldDef clPatternSpread[] = {
   { "Pad",      VSPREAD_PAD },
   { "Reflect",  VSPREAD_REFLECT },
   { "Repeat",   VSPREAD_REPEAT },
   { "ReflectX", VSPREAD_REFLECT_X },
   { "ReflectY", VSPREAD_REFLECT_Y },
   { NULL, 0 }
};

static const FieldArray clPatternFields[] = {
   { "X",            FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)PATTERN_GET_X, (APTR)PATTERN_SET_X },
   { "Y",            FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)PATTERN_GET_Y, (APTR)PATTERN_SET_Y },
   { "Width",        FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)PATTERN_GET_Width, (APTR)PATTERN_SET_Width },
   { "Height",       FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)PATTERN_GET_Height, (APTR)PATTERN_SET_Height },
   { "Opacity",      FDF_DOUBLE|FDF_RW,          0, NULL, (APTR)PATTERN_SET_Opacity },
   { "Scene",        FDF_INTEGRAL|FDF_R,         0, NULL, NULL },
   { "Inherit",      FDF_OBJECT|FDF_RW,          0, NULL, (APTR)PATTERN_SET_Inherit },
   { "SpreadMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clPatternSpread, NULL, NULL },
   { "Units",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clPatternUnits, NULL, NULL },
   { "ContentUnits", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clPatternUnits, NULL, NULL },
   { "Dimensions",   FDF_LONGFLAGS|FDF_R,        (MAXINT)&clPatternDimensions, NULL, NULL },
   //{ "AspectRatio", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, (MAXINT)&clAspectRatio, (APTR)PATTERN_GET_AspectRatio, (APTR)PATTERN_SET_AspectRatio },
   // Virtual fields
   { "Matrices",     FDF_VIRTUAL|FDF_POINTER|FDF_STRUCT|FDF_RW, (MAXINT)"VectorMatrix", (APTR)VECTORPATTERN_GET_Matrices, (APTR)VECTORPATTERN_SET_Matrices },
   { "Transform",    FDF_VIRTUAL|FDF_STRING|FDF_W, 0, NULL, (APTR)PATTERN_SET_Transform },
   { "Viewport",     FDF_VIRTUAL|FDF_OBJECT|FDF_R, ID_VECTORVIEWPORT, (APTR)PATTERN_GET_Viewport, NULL },
   END_FIELD
};

ERROR init_pattern(void) // The pattern is a definition type for creating patterns and not drawing.
{
   clVectorPattern = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTORPATTERN),
      fl::Name("VectorPattern"),
      fl::Category(CCF_GRAPHICS),
      fl::Flags(CLF_PROMOTE_INTEGRAL),
      fl::Actions(clPatternActions),
      fl::Fields(clPatternFields),
      fl::Size(sizeof(extVectorPattern)),
      fl::Path(MOD_PATH));

   return clVectorPattern ? ERR_Okay : ERR_AddClass;
}

