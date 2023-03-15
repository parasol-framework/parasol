/*********************************************************************************************************************

Please note that this is not an extension of the Vector class.  It is used for the purposes of gradient definitions only.

-CLASS-
VectorGradient: Provides support for the filling and stroking of vectors with colour gradients.

The VectorGradient class is used by Vector painting algorithms to fill and stroke vectors with gradients.  This is
achieved by initialising a VectorGradient object with the desired settings and then registering it with
a @VectorScene via the @VectorScene.AddDef() method.

Any vector within the target scene will be able to utilise the gradient for filling or stroking by referencing its
name through the @Vector.Fill and @Vector.Stroke fields.  For instance 'url(#redgradient)'.

It is strongly recommended that the VectorGradient is owned by the @VectorScene that is handling the
definition.  This will ensure that the VectorGradient is de-allocated when the scene is destroyed.

-END-

*********************************************************************************************************************/

// Return a gradient table for a vector with its opacity multiplier applied.  The table is cached with the vector so
// that it does not need to be recalculated when required again.

GRADIENT_TABLE * get_fill_gradient_table(extVector &Vector, DOUBLE Opacity)
{
   pf::Log log(__FUNCTION__);

   GradientColours *cols = ((extVectorGradient *)Vector.FillGradient)->Colours;
   if (!cols) {
      if (Vector.FillGradient->Inherit) cols = ((extVectorGradient *)Vector.FillGradient->Inherit)->Colours;
      if (!cols) {
         log.warning("No colour table referenced in fill gradient %p for vector #%d.", Vector.FillGradient, Vector.UID);
         return NULL;
      }
   }

   if (Opacity >= 1.0) { // Return the original gradient table if no translucency is applicable.
      Vector.FillGradientAlpha = 1.0;
      return &cols->table;
   }
   else {
      if ((Vector.FillGradientTable) and (Opacity IS Vector.FillGradientAlpha)) return Vector.FillGradientTable;

      delete Vector.FillGradientTable;
      Vector.FillGradientTable = new (std::nothrow) GRADIENT_TABLE();
      if (!Vector.FillGradientTable) {
         log.warning("Failed to allocate fill gradient table");
         return NULL;
      }
      Vector.FillGradientAlpha = Opacity;

      for (unsigned i=0; i < Vector.FillGradientTable->size(); i++) {
         (*Vector.FillGradientTable)[i] = agg::rgba8(cols->table[i].r, cols->table[i].g, cols->table[i].b,
            cols->table[i].a * Opacity);
      }

      return Vector.FillGradientTable;
   }
}

//********************************************************************************************************************

GRADIENT_TABLE * get_stroke_gradient_table(extVector &Vector)
{
   pf::Log log(__FUNCTION__);

   GradientColours *cols = ((extVectorGradient *)Vector.StrokeGradient)->Colours;
   if (!cols) {
      if (Vector.StrokeGradient->Inherit) cols = ((extVectorGradient *)Vector.StrokeGradient->Inherit)->Colours;
      if (!cols) {
         log.warning("No colour table referenced in stroke gradient %p for vector #%d.", Vector.FillGradient, Vector.UID);
         return NULL;
      }
   }

   if ((Vector.StrokeOpacity IS 1.0) and (Vector.Opacity IS 1.0)) {
      Vector.StrokeGradientAlpha = 1.0;
      return &cols->table;
   }
   else {
      DOUBLE opacity = Vector.StrokeOpacity * Vector.Opacity;
      if ((Vector.StrokeGradientTable) and (opacity IS Vector.StrokeGradientAlpha)) return Vector.StrokeGradientTable;

      delete Vector.StrokeGradientTable;
      Vector.StrokeGradientTable = new (std::nothrow) GRADIENT_TABLE();
      if (!Vector.StrokeGradientTable) {
         log.warning("Failed to allocate stroke gradient table");
         return NULL;
      }
      Vector.StrokeGradientAlpha = opacity;

      for (unsigned i=0; i < Vector.StrokeGradientTable->size(); i++) {
         (*Vector.StrokeGradientTable)[i] = agg::rgba8(cols->table[i].r, cols->table[i].g, cols->table[i].b, cols->table[i].a * opacity);
      }

      return Vector.StrokeGradientTable;
   }
}

//********************************************************************************************************************
// Constructor for the GradientColours class.  This expects to be called whenever the Gradient class updates the
// Stops array.

GradientColours::GradientColours(extVectorGradient *Gradient, DOUBLE Alpha)
{
   LONG stop, i1, i2, i;
   GradientStop *stops = Gradient->Stops;

   for (stop=0; stop < Gradient->TotalStops-1; stop++) {
      i1 = F2T(255.0 * stops[stop].Offset);
      if (i1 < 0) i1 = 0;
      else if (i1 > 255) i1 = 255;

      i2 = F2T(255.0 * stops[stop+1].Offset);
      if (i2 < 0) i2 = 0;
      else if (i2 > 255) i2 = 255;

      agg::rgba8 begin(stops[stop].RGB.Red*255, stops[stop].RGB.Green*255, stops[stop].RGB.Blue*255, stops[stop].RGB.Alpha * Alpha * 255);
      agg::rgba8 end(stops[stop+1].RGB.Red*255, stops[stop+1].RGB.Green*255, stops[stop+1].RGB.Blue*255, stops[stop+1].RGB.Alpha * Alpha * 255);

      if ((stop IS 0) and (i1 > 0)) {
         for (i=0; i < i1; i++) table[i] = begin;
      }

      if (i1 <= i2) {
         for (i=i1; i <= i2; i++) {
            DOUBLE j = (DOUBLE)(i - i1) / (DOUBLE)(i2-i1);
            table[i] = begin.gradient(end, j);
         }
      }

      if ((stop IS Gradient->TotalStops-2) and (i2 < 255)) {
         for (i=i2; i <= 255; i++) table[i] = end;
      }
   }
}

//********************************************************************************************************************

static ERROR VECTORGRADIENT_Free(extVectorGradient *Self, APTR Void)
{
   if (Self->ID) { FreeResource(Self->ID); Self->ID = NULL; }
   if (Self->Stops) { FreeResource(Self->Stops); Self->Stops = NULL; }
   if (Self->Colours) { delete Self->Colours; Self->Colours = NULL; }

   VectorMatrix *next;
   for (auto scan=Self->Matrices; scan; scan=next) {
      next = scan->Next;
      FreeResource(scan);
   }
   Self->Matrices = NULL;

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORGRADIENT_Init(extVectorGradient *Self, APTR Void)
{
   pf::Log log;

   if ((Self->SpreadMethod <= 0) or (Self->SpreadMethod >= VSPREAD_END)) {
      log.traceWarning("Invalid SpreadMethod value of %d", Self->SpreadMethod);
      return ERR_OutOfRange;
   }

   if ((Self->Units <= 0) or (Self->Units >= VUNIT_END)) {
      log.traceWarning("Invalid Units value of %d", Self->Units);
      return ERR_OutOfRange;
   }

   if ((Self->Type IS VGT_CONTOUR) and (Self->Units IS VUNIT_USERSPACE)) {
      log.warning("Contour gradients are not compatible with Units.USERSPACE.");
      Self->Units = VUNIT_BOUNDING_BOX;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR VECTORGRADIENT_NewObject(extVectorGradient *Self, APTR Void)
{
   Self->SpreadMethod = VSPREAD_PAD;
   Self->Type    = VGT_LINEAR;
   Self->Units   = VUNIT_BOUNDING_BOX;
   // SVG requires that these are all set to 50%
   Self->CenterX = 0.5;
   Self->CenterY = 0.5;
   Self->Radius  = 0.5;
   Self->X1      = 0;
   Self->X2      = 100; // For an effective contoured gradient, this needs to default to 100
   Self->Flags  |= VGF_RELATIVE_CX|VGF_RELATIVE_CY|VGF_RELATIVE_RADIUS;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
CenterX: The horizontal center point of the gradient.

The (CenterX,CenterY) coordinates define the center point of the gradient.  The center point will only be used if
the gradient type requires it (such as the radial type).  By default, the center point is set to 50%.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_CenterX(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->CenterX;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_CX)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_CenterX(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_CX) & (~VGF_FIXED_CX);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_CX) & (~VGF_RELATIVE_CX);

   Self->CenterX = val;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
CenterY: The vertical center point of the gradient.

The (CenterX,CenterY) coordinates define the center point of the gradient.  The center point will only be used if
the gradient type requires it (such as the radial type).  By default, the center point is set to 50%.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_CenterY(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->CenterY;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_CY)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_CenterY(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_CY) & (~VGF_FIXED_CY);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_CY) & (~VGF_RELATIVE_CY);

   Self->CenterY = val;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
ColourSpace: Defines the colour space to use when interpolating gradient colours.
Lookup: VCS

By default, gradients are rendered using the standard RGB colour space and alpha blending rules.  Changing the colour
space to `LINEAR_RGB` will force the renderer to automatically convert sRGB values to linear RGB when blending.

-FIELD-
Flags: Dimension flags are stored here.
Lookup: VGF

Dimension flags that indicate whether field values are fixed or relative are defined here.

-FIELD-
FX: The horizontal focal point for radial gradients.

The (FX,FY) coordinates define the focal point for radial gradients.  If left undefined, the focal point will match the
center of the gradient.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_FX(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->FX;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_FX)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_FX(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_FX) & (~VGF_FIXED_FX);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_FX) & (~VGF_RELATIVE_FX);

   Self->FX = val;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
FY: The vertical focal point for radial gradients.

The (FX,FY) coordinates define the focal point for radial gradients.  If left undefined, the focal point will match the
center of the gradient.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_FY(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->FY;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_FY)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_FY(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_FY) & (~VGF_FIXED_FY);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_FY) & (~VGF_RELATIVE_FY);

   Self->FY = val;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
ID: String identifier for a vector.

The ID field is provided for the purpose of SVG support.  Where possible we would recommend that you use the
existing object name and automatically assigned ID's for identifiers.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_ID(extVectorGradient *Self, STRING *Value)
{
   *Value = Self->ID;
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_ID(extVectorGradient *Self, CSTRING Value)
{
   if (Self->ID) FreeResource(Self->ID);

   if (Value) {
      Self->ID = StrClone(Value);
      Self->NumericID = StrHash(Value, TRUE);
   }
   else {
      Self->ID = NULL;
      Self->NumericID = 0;
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Inherit: Inherit attributes from the VectorGradient referenced here.

Attributes can be inherited from another gradient by referencing that gradient in this field.  This feature is provided
primarily for the purpose of simplifying SVG compatibility and its use may result in an unnecessary performance penalty.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_SET_Inherit(extVectorGradient *Self, extVectorGradient *Value)
{
   if (Value) {
      if (Value->ClassID IS ID_VECTORGRADIENT) Self->Inherit = Value;
      else return ERR_InvalidValue;
   }
   else Self->Inherit = NULL;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Matrices: A linked list of transform matrices that have been applied to the gradient.

All transforms that have been applied to the gradient can be read from the Matrices field.  Each transform is
represented by a VectorMatrix structure, and are linked in the order in which they were applied to the gradient.

&VectorMatrix

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_Matrices(extVectorGradient *Self, VectorMatrix **Value)
{
   *Value = Self->Matrices;
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_Matrices(extVectorGradient *Self, VectorMatrix *Value)
{
   if (!Value) {
      auto hook = &Self->Matrices;
      while (Value) {
         VectorMatrix *matrix;
         if (!AllocMemory(sizeof(VectorMatrix), MEM_DATA|MEM_NO_CLEAR, &matrix)) {
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

/*********************************************************************************************************************

-FIELD-
NumericID: A unique identifier for the vector.

This field assigns a numeric ID to a vector.  Alternatively it can also reflect a case-sensitive hash of the
#ID field if that has been defined previously.

If NumericID is set by the client, then any value in #ID will be immediately cleared.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_NumericID(extVectorGradient *Self, LONG *Value)
{
   *Value = Self->NumericID;
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_NumericID(extVectorGradient *Self, LONG Value)
{
   Self->NumericID = Value;
   if (Self->ID) { FreeResource(Self->ID); Self->ID = NULL; }
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Radius: The radius of the gradient.

The radius of the gradient can be defined in fixed units or relative terms to its container.  A default radius of
50% (0.5) applies if this field is not set.

The Radius value has no effect if the gradient is linear.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_Radius(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->Radius;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_RADIUS)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_Radius(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (val >= 0) {
      if (Value->Type & FD_PERCENTAGE) {
         val = val * 0.01;
         Self->Flags = (Self->Flags | VGF_RELATIVE_RADIUS) & (~VGF_FIXED_RADIUS);
      }
      else Self->Flags = (Self->Flags | VGF_FIXED_RADIUS) & (~VGF_RELATIVE_RADIUS);

      Self->Radius = val;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
SpreadMethod: The behaviour to use when the gradient bounds do not match the vector path.

Indicates what happens if the gradient starts or ends inside the bounds of the target vector.  The default is
`VSPREAD_PAD`.

-FIELD-
Stops: Defines the colours to use for the gradient.

The colours that will be used for drawing a gradient are defined by the Stops array.  At least two stops are required
to define a start and end point for interpolating the gradient colours.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_Stops(extVectorGradient *Self, GradientStop **Value, LONG *Elements)
{
   *Value    = Self->Stops;
   *Elements = Self->TotalStops;
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_Stops(extVectorGradient *Self, GradientStop *Value, LONG Elements)
{
   if (Self->Stops) { FreeResource(Self->Stops); Self->Stops = NULL; }

   if (Elements >= 2) {
      if (!AllocMemory(sizeof(GradientStop) * Elements, MEM_DATA|MEM_NO_CLEAR, &Self->Stops)) {
         Self->TotalStops = Elements;
         CopyMemory(Value, Self->Stops, Elements * sizeof(GradientStop));
         if (Self->Colours) delete Self->Colours;
         Self->Colours = new (std::nothrow) GradientColours(Self, 1.0);
         if (!Self->Colours) return ERR_AllocMemory;
         Self->ChangeCounter++;
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else {
      pf::Log log;
      log.warning("Array size %d < 2", Elements);
      return ERR_InvalidValue;
   }
}

/*********************************************************************************************************************

-FIELD-
TotalStops: Total number of stops defined in the Stops array.

This read-only field indicates the total number of stops that have been defined in the #Stops array.

-FIELD-
Transform: Applies a transform to the gradient.

A transform can be applied to the gradient by setting this field with an SVG compliant transform string.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_SET_Transform(extVectorGradient *Self, CSTRING Commands)
{
   pf::Log log;

   if (!Commands) return log.warning(ERR_InvalidValue);

   if (!Self->Matrices) {
      VectorMatrix *matrix;
      if (!AllocMemory(sizeof(VectorMatrix), MEM_DATA|MEM_NO_CLEAR, &matrix)) {
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

/*********************************************************************************************************************

-FIELD-
Type: Specifies the type of gradient (e.g. RADIAL, LINEAR)
Lookup: VGT

The type of the gradient to be drawn is specified here.

-FIELD-
Units: Defines the coordinate system for fields X1, Y1, X2 and Y2.

The default coordinate system for gradients is `BOUNDING_BOX`, which positions the gradient around the vector that
references it.  The alternative is `USERSPACE`, which positions the gradient relative to the current viewport.

-FIELD-
X1: Initial X coordinate for the gradient.

The (X1,Y1) field values define the starting coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from (X1,Y1) to (X2,Y2).

Coordinate values can be expressed as percentages that are relative to the target space.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_X1(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->X1;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_X1)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_X1(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_X1) & (~VGF_FIXED_X1);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_X1) & (~VGF_RELATIVE_X1);

   Self->X1 = val;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
X2: Final X coordinate for the gradient.

The (X2,Y2) field values define the end coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from (X1,Y1) to (X2,Y2).

Coordinate values can be expressed as percentages that are relative to the target space.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_X2(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->X2;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_X2)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_X2(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_X2) & (~VGF_FIXED_X2);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_X2) & (~VGF_RELATIVE_X2);

   Self->X2 = val;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Y1: Initial Y coordinate for the gradient.

The (X1,Y1) field values define the starting coordinate for mapping linear gradients.  Other gradient types ignore
these values.

*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_Y1(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->Y1;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_Y1)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_Y1(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_Y1) & (~VGF_FIXED_Y1);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_Y1) & (~VGF_RELATIVE_Y1);

   Self->Y1 = val;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Y2: Final Y coordinate for the gradient.

The (X2,Y2) field values define the end coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from (X1,Y1) to (X2,Y2).

Coordinate values can be expressed as percentages that are relative to the target space.
-END-
*********************************************************************************************************************/

static ERROR VECTORGRADIENT_GET_Y2(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val = Self->Y2;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Flags & VGF_RELATIVE_Y2)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR VECTORGRADIENT_SET_Y2(extVectorGradient *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Flags = (Self->Flags | VGF_RELATIVE_Y2) & (~VGF_FIXED_Y2);
   }
   else Self->Flags = (Self->Flags | VGF_FIXED_Y2) & (~VGF_RELATIVE_Y2);

   Self->Y2 = val;
   return ERR_Okay;
}

//********************************************************************************************************************

#include "gradient_def.c"

static const FieldArray clGradientFields[] = {
   { "X1",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_X1, VECTORGRADIENT_SET_X1 },
   { "Y1",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_Y1, VECTORGRADIENT_SET_Y1 },
   { "X2",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_X2, VECTORGRADIENT_SET_X2 },
   { "Y2",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_Y2, VECTORGRADIENT_SET_Y2 },
   { "CenterX",      FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_CenterX, VECTORGRADIENT_SET_CenterX },
   { "CenterY",      FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_CenterY, VECTORGRADIENT_SET_CenterY },
   { "FX",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_FX, VECTORGRADIENT_SET_FX },
   { "FY",           FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_FY, VECTORGRADIENT_SET_FY },
   { "Radius",       FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, VECTORGRADIENT_GET_Radius, VECTORGRADIENT_SET_Radius },
   { "Inherit",      FDF_OBJECT|FDF_RW, NULL, VECTORGRADIENT_SET_Inherit },
   { "SpreadMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, NULL, NULL, &clVectorGradientSpreadMethod },
   { "Units",        FDF_LONG|FDF_LOOKUP|FDF_RI, NULL, NULL, &clVectorGradientUnits },
   { "Type",         FDF_LONG|FDF_LOOKUP|FDF_RI, NULL, NULL, &clVectorGradientType },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clVectorGradientFlags },
   { "ColourSpace",  FDF_LONG|FDF_RI, NULL, NULL, &clVectorGradientColourSpace },
   { "TotalStops",   FDF_LONG|FDF_R },
   // Virtual fields
   { "Matrices",     FDF_VIRTUAL|FDF_POINTER|FDF_STRUCT|FDF_RW, VECTORGRADIENT_GET_Matrices, VECTORGRADIENT_SET_Matrices, "VectorMatrix" },
   { "NumericID",    FDF_VIRTUAL|FDF_LONG|FDF_RW, VECTORGRADIENT_GET_NumericID, VECTORGRADIENT_SET_NumericID },
   { "ID",           FDF_VIRTUAL|FDF_STRING|FDF_RW, VECTORGRADIENT_GET_ID, VECTORGRADIENT_SET_ID },
   { "Stops",        FDF_VIRTUAL|FDF_ARRAY|FDF_STRUCT|FDF_RW, VECTORGRADIENT_GET_Stops, VECTORGRADIENT_SET_Stops, "GradientStop" },
   { "Transform",    FDF_VIRTUAL|FDF_STRING|FDF_W, NULL, VECTORGRADIENT_SET_Transform },
   END_FIELD
};

//********************************************************************************************************************

ERROR init_gradient(void) // The gradient is a definition type for creating gradients and not drawing.
{
   clVectorGradient = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTORGRADIENT),
      fl::Name("VectorGradient"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clVectorGradientActions),
      fl::Fields(clGradientFields),
      fl::Size(sizeof(extVectorGradient)),
      fl::Path(MOD_PATH));

   return clVectorGradient ? ERR_Okay : ERR_AddClass;
}
