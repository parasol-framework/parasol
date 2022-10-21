/*****************************************************************************

-CLASS-
VectorSpiral: Extends the Vector class with support for spiral path generation.

The VectorSpiral class provides the necessary functionality for generating spiral paths that extend from a central
point.
-END-

*****************************************************************************/

#define MAX_SPIRAL_VERTICES 65536

typedef class plVectorSpiral : public objVector {
   public:
   DOUBLE Scale;
   DOUBLE Offset;
   DOUBLE Radius;
   DOUBLE CX, CY;
   DOUBLE Step;
   LONG Dimensions;
} objVectorSpiral;

//****************************************************************************

static void generate_spiral(objVectorSpiral *Vector)
{
   const DOUBLE cx = (Vector->Dimensions & DMF_RELATIVE_CENTER_X) ? Vector->CX * get_parent_width(Vector) : Vector->CX;
   const DOUBLE cy = (Vector->Dimensions & DMF_RELATIVE_CENTER_Y) ? Vector->CY * get_parent_height(Vector) : Vector->CY;

   DOUBLE min_x = DBL_MAX, max_x = DBL_MIN, min_y = DBL_MAX, max_y = DBL_MIN;
   DOUBLE angle = 0;
   for (int i=0; i < MAX_SPIRAL_VERTICES; i++) { // The spiral points keep generating until the max number of vertices is reached, or the radius is boundary is hit.
      DOUBLE x = (Vector->Offset + Vector->Scale * angle) * cos(angle);
      DOUBLE y = (Vector->Offset + Vector->Scale * angle) * sin(angle);

      if ((ABS(x) > Vector->Radius) or (ABS(y) > Vector->Radius)) break;

      x += cx;
      y += cy;
      if (!i) Vector->BasePath.move_to(x, y);
      else Vector->BasePath.line_to(x, y);

      if (x < min_x) min_x = x;
      if (y < min_y) min_y = y;
      if (x > max_x) max_x = x;
      if (y > max_y) max_y = y;

      angle += Vector->Step;
   }

   Vector->BX1 = min_x;
   Vector->BY1 = min_y;
   Vector->BX2 = max_x;
   Vector->BY2 = max_y;
}

//****************************************************************************

static ERROR SPIRAL_NewObject(objVectorSpiral *Self, APTR Void)
{
   Self->Radius = 100;
   Self->Step   = 0.1;
   Self->Offset = 1;
   Self->Scale  = 1;
   Self->GeneratePath = (void (*)(objVector *))&generate_spiral;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
CenterX: The horizontal center of the spiral.  Expressed as a fixed or relative coordinate.

The horizontal center of the spiral is defined here as either a fixed or relative value.
-END-
*****************************************************************************/

static ERROR SPIRAL_GET_CenterX(objVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->CX;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_CENTER_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SPIRAL_SET_CenterX(objVectorSpiral *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_X) & (~DMF_FIXED_CENTER_X);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_X) & (~DMF_RELATIVE_CENTER_X);

   Self->CX = val;

   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
CenterY: The vertical center of the spiral.  Expressed as a fixed or relative coordinate.

The vertical center of the spiral is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR SPIRAL_GET_CenterY(objVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->CY;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_CENTER_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SPIRAL_SET_CenterY(objVectorSpiral *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_Y) & (~DMF_FIXED_CENTER_Y);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_Y) & (~DMF_RELATIVE_CENTER_Y);

   Self->CY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Height: The height (vertical diameter) of the spiral.

The height of the spiral is expressed as '#Radius * 2.0'.

*****************************************************************************/

static ERROR SPIRAL_GET_Height(objVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->Radius * 2.0;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SPIRAL_SET_Height(objVectorSpiral *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR_FieldTypeMismatch);
   Self->Radius = val * 0.5;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Offset: Offset the generation of the path by a given value.

The generation of a spiral's path can be offset by specifying a positive value in the Offset field.  By default the
Offset is set to zero.

*****************************************************************************/

static ERROR SPIRAL_GET_Offset(objVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->Offset;
   return ERR_Okay;
}

static ERROR SPIRAL_SET_Offset(objVectorSpiral *Self, DOUBLE Value)
{
   if (Value >= 0.0) {
      Self->Offset = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
-FIELD-
PathLength: Calibrates the user agent's distance-along-a-path calculations with that of the author.

The author's computation of the total length of the path, in user units. This value is used to calibrate the user
agent's own distance-along-a-path calculations with that of the author. The user agent will scale all
distance-along-a-path computations by the ratio of PathLength to the user agent's own computed value for total path
length.

*****************************************************************************/

static ERROR SPIRAL_GET_PathLength(objVectorSpiral *Self, LONG *Value)
{
   *Value = Self->PathLength;
   return ERR_Okay;
}

static ERROR SPIRAL_SET_PathLength(objVectorSpiral *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PathLength = Value;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
-FIELD-
Radius: The radius of the spiral.  Expressed as a fixed or relative coordinate.

The radius of the spiral is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR SPIRAL_GET_Radius(objVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->Radius;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_RADIUS)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SPIRAL_SET_Radius(objVectorSpiral *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_RADIUS) & (~DMF_FIXED_RADIUS);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_RADIUS) & (~DMF_RELATIVE_RADIUS);

   Self->Radius = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Scale: The scale of the spiral, expressed as a multiplier.

The spiral path can be scaled by setting this field.  The points on the spiral will be scaled by being multiplied by
the scale factor.

*****************************************************************************/

static ERROR SPIRAL_GET_Scale(objVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->Scale;
   return ERR_Okay;
}

static ERROR SPIRAL_SET_Scale(objVectorSpiral *Self, DOUBLE Value)
{
   if (Value > 0.001) {
      Self->Scale = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
-FIELD-
Step: Determines the distance between each vertex in the spiral's path.

The Step value alters the distance between each vertex in the spiral path during its generation.  The default value
is 0.1.  Using larger values will create a spiral with more visible corners due to the overall reduction in vertices.

*****************************************************************************/

static ERROR SPIRAL_GET_Step(objVectorSpiral *Self, DOUBLE *Value)
{
   *Value = Self->Step;
   return ERR_Okay;
}

static ERROR SPIRAL_SET_Step(objVectorSpiral *Self, DOUBLE Value)
{
   if (Value != 0.0) {
      Self->Step = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************

-FIELD-
Width: The width (horizontal diameter) of the spiral.

The width of the spiral is expressed as '#Radius * 2.0'.
-END-

*****************************************************************************/

static ERROR SPIRAL_GET_Width(objVectorSpiral *Self, Variable *Value)
{
   DOUBLE val = Self->Radius * 2.0;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SPIRAL_SET_Width(objVectorSpiral *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return log.warning(ERR_FieldTypeMismatch);
   Self->Radius = val * 0.5;
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static const ActionArray clVectorSpiralActions[] = {
   { AC_NewObject, (APTR)SPIRAL_NewObject },
   { 0, NULL }
};

static const FieldArray clVectorSpiralFields[] = {
   { "PathLength", FDF_VIRTUAL|FDF_LONG|FDF_RW,   0, (APTR)SPIRAL_GET_PathLength, (APTR)SPIRAL_SET_PathLength },
   { "Width",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_Width,   (APTR)SPIRAL_SET_Width },
   { "Height",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_Height,  (APTR)SPIRAL_SET_Height },
   { "CenterX",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_CenterX, (APTR)SPIRAL_SET_CenterX },
   { "CenterY",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_CenterY, (APTR)SPIRAL_SET_CenterY },
   { "Radius",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_Radius,  (APTR)SPIRAL_SET_Radius },
   { "Offset",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)SPIRAL_GET_Offset, (APTR)SPIRAL_SET_Offset },
   { "Scale",      FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)SPIRAL_GET_Scale, (APTR)SPIRAL_SET_Scale },
   { "Step",       FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)SPIRAL_GET_Step, (APTR)SPIRAL_SET_Step },
   // Synonyms
   { "CX", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_CenterX, (APTR)SPIRAL_SET_CenterX },
   { "CY", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_CenterY, (APTR)SPIRAL_SET_CenterY },
   { "R",  FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)SPIRAL_GET_Radius,  (APTR)SPIRAL_SET_Radius },
   END_FIELD
};

//****************************************************************************

static ERROR init_spiral(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorSpiral,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORSPIRAL,
      FID_Name|TSTRING,      "VectorSpiral",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clVectorSpiralActions,
      FID_Fields|TARRAY,     clVectorSpiralFields,
      FID_Size|TLONG,        sizeof(objVectorSpiral),
      FID_Path|TSTR,         "modules:vector",
      TAGEND));
}

