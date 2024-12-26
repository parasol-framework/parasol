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

GRADIENT_TABLE * get_fill_gradient_table(extPainter &Painter, DOUBLE Opacity)
{
   pf::Log log(__FUNCTION__);

   GradientColours *cols = ((extVectorGradient *)Painter.Gradient)->Colours;
   if (!cols) {
      log.warning("No colour table in gradient %p.", Painter.Gradient);
      return NULL;
   }

   if (Opacity >= 1.0) { // Return the original gradient table if no translucency is applicable.
      Painter.GradientAlpha = 1.0;
      return &cols->table;
   }
   else {
      if ((Painter.GradientTable) and (Opacity IS Painter.GradientAlpha)) return Painter.GradientTable;

      delete Painter.GradientTable;
      Painter.GradientTable = new (std::nothrow) GRADIENT_TABLE();
      if (!Painter.GradientTable) {
         log.warning("Failed to allocate fill gradient table");
         return NULL;
      }
      Painter.GradientAlpha = Opacity;

      for (unsigned i=0; i < Painter.GradientTable->size(); i++) {
         (*Painter.GradientTable)[i] = agg::rgba8(cols->table[i].r, cols->table[i].g, cols->table[i].b,
            cols->table[i].a * Opacity);
      }

      return Painter.GradientTable;
   }
}

//********************************************************************************************************************

GRADIENT_TABLE * get_stroke_gradient_table(extVector &Vector)
{
   pf::Log log(__FUNCTION__);

   GradientColours *cols = ((extVectorGradient *)Vector.Stroke.Gradient)->Colours;
   if (!cols) {
      if (!cols) {
         log.warning("No colour table referenced in stroke gradient %p for vector #%d.", Vector.Stroke.Gradient, Vector.UID);
         return NULL;
      }
   }

   if ((Vector.StrokeOpacity IS 1.0) and (Vector.Opacity IS 1.0)) {
      Vector.Stroke.GradientAlpha = 1.0;
      return &cols->table;
   }
   else {
      DOUBLE opacity = Vector.StrokeOpacity * Vector.Opacity;
      if ((Vector.Stroke.GradientTable) and (opacity IS Vector.Stroke.GradientAlpha)) return Vector.Stroke.GradientTable;

      delete Vector.Stroke.GradientTable;
      Vector.Stroke.GradientTable = new (std::nothrow) GRADIENT_TABLE();
      if (!Vector.Stroke.GradientTable) {
         log.warning("Failed to allocate stroke gradient table");
         return NULL;
      }
      Vector.Stroke.GradientAlpha = opacity;

      for (unsigned i=0; i < Vector.Stroke.GradientTable->size(); i++) {
         (*Vector.Stroke.GradientTable)[i] = agg::rgba8(cols->table[i].r, cols->table[i].g, cols->table[i].b, cols->table[i].a * opacity);
      }

      return Vector.Stroke.GradientTable;
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
            if (Gradient->ColourSpace IS VCS::LINEAR_RGB) {
               table[i] = begin.linear_gradient(end, j);
            }
            else table[i] = begin.gradient(end, j);
         }
      }

      if ((stop IS Gradient->TotalStops-2) and (i2 < 255)) {
         for (i=i2; i <= 255; i++) table[i] = end;
      }
   }
}

//********************************************************************************************************************

static ERR VECTORGRADIENT_Free(extVectorGradient *Self)
{
   if (Self->ID) { FreeResource(Self->ID); Self->ID = NULL; }
   if (Self->Stops) { FreeResource(Self->Stops); Self->Stops = NULL; }
   if (Self->Colours) { delete Self->Colours; Self->Colours = NULL; }

   VectorMatrix *next;
   for (auto node=Self->Matrices; node; node=next) {
      next = node->Next;
      FreeResource(node);
   }
   Self->Matrices = NULL;

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR VECTORGRADIENT_Init(extVectorGradient *Self)
{
   pf::Log log;

   if ((LONG(Self->SpreadMethod) <= 0) or (LONG(Self->SpreadMethod) >= LONG(VSPREAD::END))) {
      log.traceWarning("Invalid SpreadMethod value of %d", Self->SpreadMethod);
      return ERR::OutOfRange;
   }

   if ((LONG(Self->Units) <= 0) or (LONG(Self->Units) >= LONG(VUNIT::END))) {
      log.traceWarning("Invalid Units value of %d", Self->Units);
      return ERR::OutOfRange;
   }

   if ((Self->Type IS VGT::CONTOUR) and (Self->Units IS VUNIT::USERSPACE)) {
      log.warning("Contour gradients are not compatible with Units.USERSPACE.");
      Self->Units = VUNIT::BOUNDING_BOX;
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR VECTORGRADIENT_NewObject(extVectorGradient *Self)
{
   Self->SpreadMethod = VSPREAD::PAD;
   Self->Type    = VGT::LINEAR;
   Self->Units   = VUNIT::BOUNDING_BOX;
   // SVG requires that these are all set to 50%
   Self->CenterX = 0.5;
   Self->CenterY = 0.5;
   Self->Radius  = 0.5;
   Self->X1      = 0;
   Self->X2      = 100; // For an effective contoured gradient, this needs to default to 100
   Self->Flags  |= VGF::SCALED_CX|VGF::SCALED_CY|VGF::SCALED_RADIUS;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
CenterX: The horizontal center point of the gradient.

The `(CenterX, CenterY)` coordinates define the center point of the gradient.  The center point will only be used if
the gradient type requires it (such as the radial type).  By default, the center point is set to `50%`.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_CenterX(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->CenterX);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_CenterX(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_CX) & (~VGF::FIXED_CX);
   else Self->Flags = (Self->Flags | VGF::FIXED_CX) & (~VGF::SCALED_CX);
   Self->CenterX = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
CenterY: The vertical center point of the gradient.

The `(CenterX, CenterY)` coordinates define the center point of the gradient.  The center point will only be used if
the gradient type requires it (such as the radial type).  By default, the center point is set to `50%`.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_CenterY(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->CenterY);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_CenterY(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_CY) & (~VGF::FIXED_CY);
   else Self->Flags = (Self->Flags | VGF::FIXED_CY) & (~VGF::SCALED_CY);

   Self->CenterY = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Colour: The default background colour to use when clipping is enabled.

The colour value in this field is applicable only when a gradient is in clip-mode - by specifying the `VSPREAD::CLIP` 
flag in #SpreadMethod.  By default, this field has an alpha value of 0 to ensure that nothing is drawn 
outside the initial bounds of the gradient.  Setting any other colour value here will otherwise 
fill-in those areas.

The Colour value is defined in floating-point RGBA format, using a range of 0 - 1.0 per component.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_Colour(extVectorGradient *Self, FLOAT **Value, LONG *Elements)
{
   *Value = (FLOAT *)&Self->Colour;
   *Elements = 4;
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_Colour(extVectorGradient *Self, FLOAT *Value, LONG Elements)
{
   pf::Log log;
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
      else return log.warning(ERR::InvalidValue);
   }
   else Self->Colour.Alpha = 0;
   return ERR::Okay;
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

Dimension flags that indicate whether field values are fixed or scaled are defined here.

-FIELD-
FocalRadius: The size of the focal radius for radial gradients.

If a radial gradient has a defined focal point (by setting #FocalX and #FocalY) then the FocalRadius can be used to
adjust the size of the focal area.  The default of zero ensures that the focal area matches that defined by #Radius,
which is the standard maintained by SVG.

The FocalRadius value has no effect if the gradient is linear.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_FocalRadius(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->FocalRadius);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_FocalRadius(extVectorGradient *Self, Unit &Value)
{
   if (Value >= 0) {
      if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_FOCAL_RADIUS) & (~VGF::FIXED_FOCAL_RADIUS);
      else Self->Flags = (Self->Flags | VGF::FIXED_FOCAL_RADIUS) & (~VGF::SCALED_FOCAL_RADIUS);

      Self->FocalRadius = Value;
      return ERR::Okay;
   }
   else return ERR::OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
FocalX: The horizontal focal point for radial gradients.

The `(FocalX, FocalY)` coordinates define the focal point for radial gradients.  If left undefined, the focal point 
will match the center of the gradient.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_FocalX(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->FocalX);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_FocalX(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_FX) & (~VGF::FIXED_FX);
   else Self->Flags = (Self->Flags | VGF::FIXED_FX) & (~VGF::SCALED_FX);

   Self->FocalX = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
FocalY: The vertical focal point for radial gradients.

The `(FocalX, FocalY)` coordinates define the focal point for radial gradients.  If left undefined, the focal point 
will match the center of the gradient.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_FocalY(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->FocalY);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_FocalY(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_FY) & (~VGF::FIXED_FY);
   else Self->Flags = (Self->Flags | VGF::FIXED_FY) & (~VGF::SCALED_FY);

   Self->FocalY = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
ID: String identifier for a vector.

The ID field is provided for the purpose of SVG support.  Where possible, we recommend that you use the
existing object name and automatically assigned ID's for identifiers.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_ID(extVectorGradient *Self, STRING *Value)
{
   *Value = Self->ID;
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_ID(extVectorGradient *Self, CSTRING Value)
{
   if (Self->ID) FreeResource(Self->ID);

   if (Value) {
      Self->ID = strclone(Value);
      Self->NumericID = strhash(Value);
   }
   else {
      Self->ID = NULL;
      Self->NumericID = 0;
   }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Matrices: A linked list of transform matrices that have been applied to the gradient.

All transforms that have been applied to the gradient can be read from the Matrices field.  Each transform is
represented by a !VectorMatrix structure, and are linked in the order in which they were applied to the gradient.

!VectorMatrix

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_Matrices(extVectorGradient *Self, VectorMatrix **Value)
{
   *Value = Self->Matrices;
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_Matrices(extVectorGradient *Self, VectorMatrix *Value)
{
   if (!Value) {
      auto hook = &Self->Matrices;
      while (Value) {
         VectorMatrix *matrix;
         if (AllocMemory(sizeof(VectorMatrix), MEM::DATA|MEM::NO_CLEAR, &matrix) IS ERR::Okay) {
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
         else return ERR::AllocMemory;

         Value = Value->Next;
      }
   }
   else {
      VectorMatrix *next;
      for (auto node=Self->Matrices; node; node=next) {
         next = node->Next;
         FreeResource(node);
      }
      Self->Matrices = NULL;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
NumericID: A unique identifier for the vector.

This field assigns a numeric ID to a vector.  Alternatively it can also reflect a case-sensitive hash of the
#ID field if that has been defined previously.

If NumericID is set by the client, then any value in #ID will be immediately cleared.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_NumericID(extVectorGradient *Self, LONG *Value)
{
   *Value = Self->NumericID;
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_NumericID(extVectorGradient *Self, LONG Value)
{
   Self->NumericID = Value;
   if (Self->ID) { FreeResource(Self->ID); Self->ID = NULL; }
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Radius: The radius of the gradient.

The radius of the gradient can be defined as a fixed unit or scaled relative to its container.  A default radius of
50% (0.5) applies if this field is not set.

The Radius value has no effect if the gradient is linear.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_Radius(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->Radius);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_Radius(extVectorGradient *Self, Unit &Value)
{
   if (Value >= 0) {
      if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_RADIUS) & (~VGF::FIXED_RADIUS);
      else Self->Flags = (Self->Flags | VGF::FIXED_RADIUS) & (~VGF::SCALED_RADIUS);

      Self->Radius = Value;
      return ERR::Okay;
   }
   else return ERR::OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
SpreadMethod: The behaviour to use when the gradient bounds do not match the vector path.

Indicates what happens if the gradient starts or ends inside the bounds of the target vector.  The default is
`VSPREAD::PAD`.  Other valid options for gradients are `REFLECT`, `REPEAT` and `CLIP`.

-FIELD-
Stops: Defines the colours to use for the gradient.

The colours that will be used for drawing a gradient are defined by the Stops array.  At least two stops are required
to define a start and end point for interpolating the gradient colours.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_Stops(extVectorGradient *Self, GradientStop **Value, LONG *Elements)
{
   *Value    = Self->Stops;
   *Elements = Self->TotalStops;
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_Stops(extVectorGradient *Self, GradientStop *Value, LONG Elements)
{
   if (Self->Stops) { FreeResource(Self->Stops); Self->Stops = NULL; }

   if (Elements >= 2) {
      if (AllocMemory(sizeof(GradientStop) * Elements, MEM::DATA|MEM::NO_CLEAR, &Self->Stops) IS ERR::Okay) {
         Self->TotalStops = Elements;
         copymem(Value, Self->Stops, Elements * sizeof(GradientStop));
         if (Self->Colours) delete Self->Colours;
         Self->Colours = new (std::nothrow) GradientColours(Self, 1.0);
         if (!Self->Colours) return ERR::AllocMemory;
         Self->ChangeCounter++;
         return ERR::Okay;
      }
      else return ERR::AllocMemory;
   }
   else {
      pf::Log log;
      log.warning("Array size %d < 2", Elements);
      return ERR::InvalidValue;
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

static ERR VECTORGRADIENT_SET_Transform(extVectorGradient *Self, CSTRING Commands)
{
   pf::Log log;

   if (!Commands) return log.warning(ERR::InvalidValue);

   if (!Self->Matrices) {
      VectorMatrix *matrix;
      if (AllocMemory(sizeof(VectorMatrix), MEM::DATA|MEM::NO_CLEAR, &matrix) IS ERR::Okay) {
         matrix->Vector = NULL;
         matrix->Next   = Self->Matrices;
         matrix->ScaleX = 1.0;
         matrix->ScaleY = 1.0;
         matrix->ShearX = 0;
         matrix->ShearY = 0;
         matrix->TranslateX = 0;
         matrix->TranslateY = 0;

         Self->Matrices = matrix;
         return vec::ParseTransform(Self->Matrices, Commands);
      }
      else return ERR::AllocMemory;
   }
   else {
      vec::ResetMatrix(Self->Matrices);
      return vec::ParseTransform(Self->Matrices, Commands);
   }
}

/*********************************************************************************************************************

-FIELD-
Type: Specifies the type of gradient (e.g. `RADIAL`, `LINEAR`)
Lookup: VGT

The type of the gradient to be drawn is specified here.

-FIELD-
Units: Defines the coordinate system for #X1, #Y1, #X2 and #Y2.

The default coordinate system for gradients is `BOUNDING_BOX`, which positions the gradient around the vector that
references it.  The alternative is `USERSPACE`, which positions the gradient scaled to the current viewport.

-FIELD-
X1: Initial X coordinate for the gradient.

The `(X1, Y1)` field values define the starting coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from `(X1, Y1)` to `(X2, Y2)`.

Coordinate values can be expressed as percentages that are scaled to the target space.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_X1(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->X1);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_X1(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_X1) & (~VGF::FIXED_X1);
   else Self->Flags = (Self->Flags | VGF::FIXED_X1) & (~VGF::SCALED_X1);
   Self->X1 = Value;
   Self->CalcAngle = true;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
X2: Final X coordinate for the gradient.

The `(X2, Y2)` field values define the end coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from `(X1, Y1)` to `(X2, Y2)`.

Coordinate values can be expressed as percentages that are scaled to the target space.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_X2(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->X2);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_X2(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_X2) & (~VGF::FIXED_X2);
   else Self->Flags = (Self->Flags | VGF::FIXED_X2) & (~VGF::SCALED_X2);
   Self->X2 = Value;
   Self->CalcAngle = true;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Y1: Initial Y coordinate for the gradient.

The `(X1, Y1)` field values define the starting coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from `(X1, Y1)` to `(X2, Y2)`.

Coordinate values can be expressed as percentages that are scaled to the target space.

*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_Y1(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->Y1);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_Y1(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_Y1) & (~VGF::FIXED_Y1);
   else Self->Flags = (Self->Flags | VGF::FIXED_Y1) & (~VGF::SCALED_Y1);
   Self->Y1 = Value;
   Self->CalcAngle = true;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Y2: Final Y coordinate for the gradient.

The `(X2, Y2)` field values define the end coordinate for mapping linear gradients.  Other gradient types ignore
these values.  The gradient will be drawn from `(X1, Y1)` to `(X2, Y2)`.

Coordinate values can be expressed as percentages that are scaled to the target space.
-END-
*********************************************************************************************************************/

static ERR VECTORGRADIENT_GET_Y2(extVectorGradient *Self, Unit *Value)
{
   Value->set(Self->Y2);
   return ERR::Okay;
}

static ERR VECTORGRADIENT_SET_Y2(extVectorGradient *Self, Unit &Value)
{
   if (Value.scaled()) Self->Flags = (Self->Flags | VGF::SCALED_Y2) & (~VGF::FIXED_Y2);
   else Self->Flags = (Self->Flags | VGF::FIXED_Y2) & (~VGF::SCALED_Y2);
   Self->Y2 = Value;
   Self->CalcAngle = true;
   return ERR::Okay;
}

//********************************************************************************************************************

#include "gradient_def.c"

static const FieldArray clGradientFields[] = {
   { "X1",           FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_X1, VECTORGRADIENT_SET_X1 },
   { "Y1",           FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_Y1, VECTORGRADIENT_SET_Y1 },
   { "X2",           FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_X2, VECTORGRADIENT_SET_X2 },
   { "Y2",           FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_Y2, VECTORGRADIENT_SET_Y2 },
   { "CenterX",      FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_CenterX, VECTORGRADIENT_SET_CenterX },
   { "CenterY",      FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_CenterY, VECTORGRADIENT_SET_CenterY },
   { "FocalX",       FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_FocalX, VECTORGRADIENT_SET_FocalX },
   { "FocalY",       FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_FocalY, VECTORGRADIENT_SET_FocalY },
   { "Radius",       FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_Radius, VECTORGRADIENT_SET_Radius },
   { "FocalRadius",  FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_FocalRadius, VECTORGRADIENT_SET_FocalRadius },
   { "SpreadMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, NULL, NULL, &clVectorGradientSpreadMethod },
   { "Units",        FDF_LONG|FDF_LOOKUP|FDF_RI, NULL, NULL, &clVectorGradientUnits },
   { "Type",         FDF_LONG|FDF_LOOKUP|FDF_RW, NULL, NULL, &clVectorGradientType },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clVectorGradientFlags },
   { "ColourSpace",  FDF_LONG|FDF_RI, NULL, NULL, &clVectorGradientColourSpace },
   { "TotalStops",   FDF_LONG|FDF_R },
   // Virtual fields
   { "Colour",       FDF_VIRTUAL|FD_FLOAT|FDF_ARRAY|FD_RW, VECTORGRADIENT_GET_Colour, VECTORGRADIENT_SET_Colour },
   { "CX",           FDF_VIRTUAL|FDF_SYNONYM|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_CenterX, VECTORGRADIENT_SET_CenterX },
   { "CY",           FDF_VIRTUAL|FDF_SYNONYM|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_CenterY, VECTORGRADIENT_SET_CenterY },
   { "FX",           FDF_VIRTUAL|FDF_SYNONYM|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_FocalX, VECTORGRADIENT_SET_FocalX },
   { "FY",           FDF_VIRTUAL|FDF_SYNONYM|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, VECTORGRADIENT_GET_FocalY, VECTORGRADIENT_SET_FocalY },
   { "Matrices",     FDF_VIRTUAL|FDF_POINTER|FDF_STRUCT|FDF_RW, VECTORGRADIENT_GET_Matrices, VECTORGRADIENT_SET_Matrices, "VectorMatrix" },
   { "NumericID",    FDF_VIRTUAL|FDF_LONG|FDF_RW, VECTORGRADIENT_GET_NumericID, VECTORGRADIENT_SET_NumericID },
   { "ID",           FDF_VIRTUAL|FDF_STRING|FDF_RW, VECTORGRADIENT_GET_ID, VECTORGRADIENT_SET_ID },
   { "Stops",        FDF_VIRTUAL|FDF_ARRAY|FDF_STRUCT|FDF_RW, VECTORGRADIENT_GET_Stops, VECTORGRADIENT_SET_Stops, "GradientStop" },
   { "Transform",    FDF_VIRTUAL|FDF_STRING|FDF_W, NULL, VECTORGRADIENT_SET_Transform },
   END_FIELD
};

//********************************************************************************************************************

ERR init_gradient(void) // The gradient is a definition type for creating gradients and not drawing.
{
   clVectorGradient = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::VECTORGRADIENT),
      fl::Name("VectorGradient"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clVectorGradientActions),
      fl::Fields(clGradientFields),
      fl::Size(sizeof(extVectorGradient)),
      fl::Path(MOD_PATH));

   return clVectorGradient ? ERR::Okay : ERR::AddClass;
}
