/*********************************************************************************************************************

-CLASS-
VectorSpiral: Extends the Vector class with support for spiral path generation.

The VectorSpiral class generates spiral paths that extend from a central point.
-END-

*********************************************************************************************************************/

#define MAX_SPIRAL_VERTICES 65536

class extVectorSpiral : public extVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORSPIRAL;
   static constexpr CSTRING CLASS_NAME = "VectorSpiral";
   using create = pf::Create<extVectorSpiral>;

   DOUBLE Spacing;
   DOUBLE Offset;
   DOUBLE Radius;
   DOUBLE CX, CY;
   DOUBLE Step;
   DOUBLE LoopLimit;
   LONG Dimensions;
};

//********************************************************************************************************************

static void generate_spiral(extVectorSpiral *Vector, agg::path_storage &Path)
{
   const DOUBLE cx = (Vector->Dimensions & DMF_SCALED_CENTER_X) ? Vector->CX * get_parent_width(Vector) : Vector->CX;
   const DOUBLE cy = (Vector->Dimensions & DMF_SCALED_CENTER_Y) ? Vector->CY * get_parent_height(Vector) : Vector->CY;

   DOUBLE min_x = DBL_MAX, max_x = -DBL_MAX, min_y = DBL_MAX, max_y = -DBL_MAX;
   DOUBLE angle  = 0;
   DOUBLE radius = Vector->Offset;
   DOUBLE limit  = Vector->LoopLimit * 360.0;
   DOUBLE max_radius = Vector->Radius ? Vector->Radius : DBL_MAX;
   DOUBLE lx = -DBL_MAX, ly = -DBL_MAX;
   DOUBLE step = Vector->Step;

   if (step > 180) step = 180;
   else if (step < 0.1) step = 0.1;

   if ((max_radius IS DBL_MAX) and (limit <= 0.01)) limit = 360;
   else if (limit < 0.001) limit = DBL_MAX; // Ignore the loop limit in favour of radius limit

   for (int v=0; (v < MAX_SPIRAL_VERTICES) and (angle < limit) and (radius < max_radius); v++) {
      DOUBLE x = radius * cos(angle * DEG2RAD);
      DOUBLE y = radius * sin(angle * DEG2RAD);

      x += cx;
      y += cy;
      if ((std::abs(x - lx) >= 1.0) or (std::abs(y - ly) >= 1.0)) { // Only record a vertex if its position has significantly changed from the last
         if (!v) Path.move_to(x, y); // First vertex
         else Path.line_to(x, y);
         lx = x;
         ly = y;
      }

      // Boundary management

      if (x < min_x) min_x = x;
      if (y < min_y) min_y = y;
      if (x > max_x) max_x = x;
      if (y > max_y) max_y = y;

      // These computations control the radius, effectively changing the rate at which the spiral expands.

      if (Vector->Spacing) radius = Vector->Offset + (Vector->Spacing * (angle / 360.0));
      else radius += step * 0.1;

      // Increment the angle by the step.  A high step value results in a jagged spiral.

      angle += step;
   }

   Vector->Bounds = { min_x, min_y, max_x, max_y };
}

//********************************************************************************************************************

static ERR SPIRAL_NewObject(extVectorSpiral *Self)
{
   Self->Step   = 1.0;
   Self->GeneratePath = (void (*)(extVector *, agg::path_storage &))&generate_spiral;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterX: The horizontal center of the spiral.  Expressed as a fixed or scaled coordinate.

The horizontal center of the spiral is defined here as either a fixed or scaled value.
-END-
*********************************************************************************************************************/

static ERR SPIRAL_GET_CenterX(extVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->CX;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR SPIRAL_SET_CenterX(extVectorSpiral *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR::FieldTypeMismatch);

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_CENTER_X) & (~DMF_FIXED_CENTER_X);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_X) & (~DMF_SCALED_CENTER_X);

   Self->CX = val;

   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterY: The vertical center of the spiral.  Expressed as a fixed or scaled coordinate.

The vertical center of the spiral is defined here as either a fixed or scaled value.

*********************************************************************************************************************/

static ERR SPIRAL_GET_CenterY(extVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->CY;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR SPIRAL_SET_CenterY(extVectorSpiral *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR::FieldTypeMismatch);

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_CENTER_Y) & (~DMF_FIXED_CENTER_Y);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_Y) & (~DMF_SCALED_CENTER_Y);

   Self->CY = val;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
LoopLimit: Used to limit the number of loops produced by the spiral path generator.

The LoopLimit can be used to impose a limit on the total number of loops that are performed by the spiral path
generator.  It can be used as an alternative to, or conjunction with the #Radius value to limit the final spiral size.

If the LoopLimit is not set, the #Radius will take precedence.

*********************************************************************************************************************/

static ERR SPIRAL_GET_LoopLimit(extVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->LoopLimit;
   return ERR::Okay;
}

static ERR SPIRAL_SET_LoopLimit(extVectorSpiral *Self, DOUBLE Value)
{
   if (Value >= 0) {
      Self->LoopLimit = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Spacing: Declares the amount of empty space between each loop of the spiral.

Spacing tightly controls the computation of the spiral path, ensuring that a specific amount of empty space is left
between each loop.  The space is declared in pixel units.

If Spacing is undeclared, the spiral expands at an incremental rate of `Step * 0.1`.

*********************************************************************************************************************/

static ERR SPIRAL_GET_Spacing(extVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->Spacing;
   return ERR::Okay;
}

static ERR SPIRAL_SET_Spacing(extVectorSpiral *Self, DOUBLE Value)
{
   if (Value >= 0.0) {
      Self->Spacing = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Height: The height (vertical diameter) of the spiral.

The height of the spiral is expressed as `Radius * 2.0`.

*********************************************************************************************************************/

static ERR SPIRAL_GET_Height(extVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->Radius * 2.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR SPIRAL_SET_Height(extVectorSpiral *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR::FieldTypeMismatch);
   Self->Radius = val * 0.5;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Offset: Offset the starting coordinate of the spiral by this value.

The generation of a spiral's path can be offset by specifying a positive value in the Offset field.  By default the
Offset is set to zero.

*********************************************************************************************************************/

static ERR SPIRAL_GET_Offset(extVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->Offset;
   return ERR::Okay;
}

static ERR SPIRAL_SET_Offset(extVectorSpiral *Self, DOUBLE Value)
{
   if (Value >= 0.0) {
      Self->Offset = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
PathLength: Calibrates the user agent's distance-along-a-path calculations with that of the author.

The author's computation of the total length of the path, in user units. This value is used to calibrate the user
agent's own distance-along-a-path calculations with that of the author. The user agent will scale all
distance-along-a-path computations by the ratio of PathLength to the user agent's own computed value for total path
length.

*********************************************************************************************************************/

static ERR SPIRAL_GET_PathLength(extVectorSpiral *Self, LONG *Value)
{
   *Value = Self->PathLength;
   return ERR::Okay;
}

static ERR SPIRAL_SET_PathLength(extVectorSpiral *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PathLength = Value;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Radius: The radius of the spiral.  Expressed as a fixed or scaled coordinate.

The radius of the spiral is defined here as either a fixed or scaled value.  If zero, preference is given to
#LoopLimit.

*********************************************************************************************************************/

static ERR SPIRAL_GET_Radius(extVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->Radius;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR SPIRAL_SET_Radius(extVectorSpiral *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR::FieldTypeMismatch);

   if (val < 0) return log.warning(ERR::InvalidDimension);

   if (Value->Type & FD_SCALED) Self->Dimensions = (Self->Dimensions | DMF_SCALED_RADIUS) & (~DMF_FIXED_RADIUS);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_RADIUS) & (~DMF_SCALED_RADIUS);

   Self->Radius = val;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Step: Determines the distance between each vertex in the spiral's path.

The Step value affects the distance between each vertex in the spiral path during its generation.  The default value
is `1.0`.  Using larger values will create a spiral with jagged corners due to the reduction in vertices.

*********************************************************************************************************************/

static ERR SPIRAL_GET_Step(extVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->Step;
   return ERR::Okay;
}

static ERR SPIRAL_SET_Step(extVectorSpiral *Self, DOUBLE Value)
{
   if (Value != 0.0) {
      Self->Step = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
Width: The width (horizontal diameter) of the spiral.

The width of the spiral is expressed as `Radius * 2.0`.
-END-

*********************************************************************************************************************/

static ERR SPIRAL_GET_Width(extVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->Radius * 2.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR::Okay;
}

static ERR SPIRAL_SET_Width(extVectorSpiral *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR::FieldTypeMismatch);

   Self->Radius = val * 0.5;
   reset_path(Self);
   return ERR::Okay;
}

//********************************************************************************************************************

static const ActionArray clVectorSpiralActions[] = {
   { AC_NewObject, SPIRAL_NewObject },
   { 0, NULL }
};

static const FieldArray clVectorSpiralFields[] = {
   { "PathLength", FDF_VIRTUAL|FDF_LONG|FDF_RW, SPIRAL_GET_PathLength, SPIRAL_SET_PathLength },
   { "Width",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_Width,   SPIRAL_SET_Width },
   { "Height",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_Height,  SPIRAL_SET_Height },
   { "CenterX",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_CenterX, SPIRAL_SET_CenterX },
   { "CenterY",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_CenterY, SPIRAL_SET_CenterY },
   { "Radius",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_Radius,  SPIRAL_SET_Radius },
   { "Offset",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SPIRAL_GET_Offset, SPIRAL_SET_Offset },
   { "Step",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SPIRAL_GET_Step, SPIRAL_SET_Step },
   { "Spacing",    FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SPIRAL_GET_Spacing, SPIRAL_SET_Spacing },
   { "LoopLimit",  FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SPIRAL_GET_LoopLimit, SPIRAL_SET_LoopLimit },
   // Synonyms
   { "CX", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_CenterX, SPIRAL_SET_CenterX },
   { "CY", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_CenterY, SPIRAL_SET_CenterY },
   { "R",  FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, SPIRAL_GET_Radius,  SPIRAL_SET_Radius },
   END_FIELD
};

//********************************************************************************************************************

static ERR init_spiral(void)
{
   clVectorSpiral = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::ClassID(ID_VECTORSPIRAL),
      fl::Name("VectorSpiral"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clVectorSpiralActions),
      fl::Fields(clVectorSpiralFields),
      fl::Size(sizeof(extVectorSpiral)),
      fl::Path(MOD_PATH));

   return clVectorSpiral ? ERR::Okay : ERR::AddClass;
}

