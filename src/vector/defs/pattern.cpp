/*****************************************************************************

Please note that this is not an extension of the Vector class.  It is used for the purposes of pattern definitions only.

-CLASS-
VectorPattern: Provides support for the filling and stroking of vectors with patterns.

The VectorPattern class is used by Vector painting algorithms to fill and stroke vectors with pre-rendered patterns.
This is achieved by initialising a VectorPattern object with the desired settings and then registering it with
a @VectorScene via the <method class="VectorScene">AddDef</> method.

Any vector within the target scene will be able to utilise the pattern for filling or stroking by referencing its
name through the @Vector.Fill and @Vector.Stroke fields.  For instance 'url(#dots)'.

It is strongly recommended that the VectorPattern is owned by the @VectorScene that is handling the
definition.  This will ensure that the VectorPattern is deallocated when the scene is destroyed.
-END-

NOTE: The VectorPattern inherits attributes from the VectorScene, which is used to define the size of the pattern and
contains the pattern content.

*****************************************************************************/

static ERROR PATTERN_Draw(objVectorPattern *Self, struct acDraw *Args)
{
   if (Self->Bitmap) {
      if ((Self->Scene->PageWidth != Self->Bitmap->Width) or (Self->Scene->PageHeight != Self->Bitmap->Height)) {
         acResize(Self->Bitmap, Self->Scene->PageWidth, Self->Scene->PageHeight, 32);
      }
   }
   else if (CreateObject(ID_BITMAP, NF_INTEGRAL, &Self->Bitmap,
      FID_Width|TLONG,  Self->Scene->PageWidth,
      FID_Height|TLONG, Self->Scene->PageHeight,
      FID_BitsPerPixel|TLONG, 32,
      FID_Flags|TLONG, BMF_ALPHA_CHANNEL,
      TAGEND)) return ERR_CreateObject;

   ClearMemory(Self->Bitmap->Data, Self->Bitmap->LineWidth * Self->Bitmap->Height);
   Self->Scene->Bitmap = Self->Bitmap;
   acDraw(Self->Scene);

   return ERR_Okay;
}

//****************************************************************************

static ERROR PATTERN_Free(objVectorPattern *Self, APTR Void)
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

static ERROR PATTERN_Init(objVectorPattern *Self, APTR Void)
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

   if (acInit(Self->Scene)) return ERR_Init;
   if (acInit(Self->Viewport)) return ERR_Init;

   return ERR_Okay;
}

//****************************************************************************

static ERROR PATTERN_NewObject(objVectorPattern *Self, APTR Void)
{
   if (!NewObject(ID_VECTORSCENE, NF_INTEGRAL, &Self->Scene)) {
      if (!NewObject(ID_VECTORVIEWPORT, 0, &Self->Viewport)) {
         SetOwner(Self->Viewport, Self->Scene);
         Self->Scene->PageWidth  = 1;
         Self->Scene->PageHeight = 1;
         Self->SpreadMethod = VSPREAD_REPEAT;
         Self->Units        = VUNIT_BOUNDING_BOX;
         Self->ContentUnits = VUNIT_USERSPACE;
         Self->SpreadMethod = VSPREAD_PAD;
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
Inherit: Inherit attributes from a VectorPattern referenced here.

Attributes can be inherited from another pattern by referencing it in this field.  This feature is provided
primarily for the purpose of simplifying SVG compatibility and its use may result in an unnecessary performance
penalty.

*****************************************************************************/

static ERROR PATTERN_SET_Inherit(objVectorPattern *Self, objVectorPattern *Value)
{
   if (Value) {
      if (Value->Head.ClassID IS ID_VECTORPATTERN) {
         Self->Inherit = Value;
      }
      else return ERR_InvalidValue;
   }
   else Self->Inherit = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Opacity: The opacity of the pattern.

The opacity of the pattern is defined as a value between 0.0 and 1.0, with 1.0 being fully opaque.  The default value
is 1.0.

*****************************************************************************/

static ERROR PATTERN_SET_Opacity(objVectorPattern *Self, DOUBLE Value)
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

The PageWidth and PageHeight fields in the VectorScene object will define the size of the generated pattern.  It is
essential that they are set prior to initialisation.

-FIELD-
SpreadMethod: The behaviour to use when the pattern bounds do not match the vector path.

Indicates what happens if the pattern starts or ends inside the bounds of the target vector.  The default value is PAD.

-FIELD-
Transform: Applies a transform to the pattern during the render process.

A transform can be applied to the pattern by setting this field with an SVG compliant transform string.

*****************************************************************************/

static ERROR PATTERN_SET_Transform(objVectorPattern *Self, CSTRING Commands)
{
   parasol::Log log;

   if (!Commands) return log.warning(ERR_InvalidValue);

   if (!Self->Matrices) {
      VectorMatrix *matrix;
      if (!vecNewMatrix(Self, &matrix)) return vecParseTransform(matrix, Commands);
      else return ERR_CreateResource;
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
BOUNDING_BOX.

-FIELD-
Viewport: Refers to the viewport that contains the pattern.

The Viewport refers to a @VectorViewport object that is created to host the vectors for the rendered pattern.  If the
Viewport does not contain at least one vector that renders an image, the pattern will be ineffective.

-FIELD-
X: X coordinate for the pattern.

The (X,Y) field values define the starting coordinate for mapping patterns.

*****************************************************************************/

static ERROR PATTERN_GET_X(objVectorPattern *Self, Variable *Value)
{
   DOUBLE val = Self->X;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR PATTERN_SET_X(objVectorPattern *Self, Variable *Value)
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

static ERROR PATTERN_GET_Y(objVectorPattern *Self, Variable *Value)
{
   DOUBLE val = Self->Y;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR PATTERN_SET_Y(objVectorPattern *Self, Variable *Value)
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
   { "Opacity",      FDF_DOUBLE|FDF_RW,          0, NULL, (APTR)PATTERN_SET_Opacity },
   { "Scene",        FDF_INTEGRAL|FDF_R,         0, NULL, NULL },
   { "Viewport",     FDF_OBJECT|FDF_R,           0, NULL, NULL },
   { "Inherit",      FDF_OBJECT|FDF_RW,          0, NULL, (APTR)PATTERN_SET_Inherit },
   { "SpreadMethod", FDF_LONG|FDF_RW,            (MAXINT)&clPatternSpread, NULL, NULL },
   { "Units",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clPatternUnits, NULL, NULL },
   { "ContentUnits", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clPatternUnits, NULL, NULL },
   { "Dimensions",   FDF_LONGFLAGS|FDF_R,        (MAXINT)&clPatternDimensions, NULL, NULL },
   //{ "AspectRatio", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, (MAXINT)&clAspectRatio, (APTR)PATTERN_GET_AspectRatio, (APTR)PATTERN_SET_AspectRatio },
   // Virtual fields
   { "Transform",    FDF_VIRTUAL|FDF_STRING|FDF_W, 0, NULL, (APTR)PATTERN_SET_Transform },
   END_FIELD
};

static ERROR init_pattern(void) // The pattern is a definition type for creating patterns and not drawing.
{
   return(CreateObject(ID_METACLASS, 0, &clVectorPattern,
      FID_BaseClassID|TLONG, ID_VECTORPATTERN,
      FID_Name|TSTRING,      "VectorPattern",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clPatternActions,
      FID_Fields|TARRAY,     clPatternFields,
      FID_Flags|TLONG,       CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Size|TLONG,        sizeof(objVectorPattern),
      FID_Path|TSTR,         "modules:vector",
      TAGEND));
}

