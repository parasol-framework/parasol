/*********************************************************************************************************************

-CLASS-
VectorRectangle: Extends the Vector class with support for generating rectangles.

VectorRectangle extends the @Vector class with the ability to generate rectangular paths.

-END-

*********************************************************************************************************************/

#include "agg_rounded_rect.h"

static void generate_rectangle(extVectorRectangle *Vector)
{
   DOUBLE width = Vector->rWidth, height = Vector->rHeight;
   DOUBLE x = Vector->rX, y = Vector->rY;

   if (Vector->rDimensions & (DMF_RELATIVE_X|DMF_RELATIVE_Y)) {
      if (Vector->rDimensions & DMF_RELATIVE_X) x *= get_parent_width(Vector);
      if (Vector->rDimensions & DMF_RELATIVE_Y) y *= get_parent_height(Vector);
   }

   if (Vector->rDimensions & (DMF_RELATIVE_WIDTH|DMF_RELATIVE_HEIGHT)) {
      if (Vector->rDimensions & DMF_RELATIVE_WIDTH) width *= get_parent_width(Vector);
      if (Vector->rDimensions & DMF_RELATIVE_HEIGHT) height *= get_parent_height(Vector);
   }

   if (Vector->rRoundX > 0) {
      // SVG rules that RX will also apply to RY unless RY != 0.
      // An RX of zero disables rounding (contrary to SVG).
      // If RX is greater than width/2, set RX to width/2.  Same for RY on the vertical axis.

      DOUBLE rx = Vector->rRoundX, ry = Vector->rRoundY;

      if (Vector->rDimensions & DMF_RELATIVE_RADIUS_X) {
         rx *= sqrt((width * width) + (height * height)) * INV_SQRT2;
      }

      if (rx > width * 0.5) rx = width * 0.5; // SVG rule

      if ((rx != ry) and (ry)) {
         if (Vector->rDimensions & DMF_RELATIVE_RADIUS_Y) {
            ry *= sqrt((width * width) + (height * height)) * INV_SQRT2;
         }
         if (ry > height * 0.5) ry = height * 0.5;
      }
      else ry = rx;

      agg::rounded_rect aggrect(x, y, x + width, y + height, rx, ry);
      aggrect.normalize_radius(); // Required because???

      Vector->BasePath.concat_path(aggrect);
   }
   else {
      Vector->BasePath.move_to(x, y);
      Vector->BasePath.line_to(x+width, y);
      Vector->BasePath.line_to(x+width, y+height);
      Vector->BasePath.line_to(x, y+height);
      Vector->BasePath.close_polygon();
   }

   Vector->BX1 = x;
   Vector->BY1 = y;
   Vector->BX2 = x + width;
   Vector->BY2 = y + height;
}

/*********************************************************************************************************************
-ACTION-
Move: Moves the vector to a new position.

*********************************************************************************************************************/

static ERROR RECTANGLE_Move(extVectorRectangle *Self, struct acMove *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   Self->rX += Args->DeltaX;
   Self->rY += Args->DeltaY;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Moves the vector to a new fixed position.

*********************************************************************************************************************/

static ERROR RECTANGLE_MoveToPoint(extVectorRectangle *Self, struct acMoveToPoint *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Args->Flags & MTF::X) != MTF::NIL) Self->rX = Args->X;
   if ((Args->Flags & MTF::Y) != MTF::NIL) Self->rY = Args->Y;
   if ((Args->Flags & MTF::RELATIVE) != MTF::NIL) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_X | DMF_RELATIVE_Y) & ~(DMF_FIXED_X | DMF_FIXED_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X | DMF_FIXED_Y) & ~(DMF_RELATIVE_X | DMF_RELATIVE_Y);
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR RECTANGLE_NewObject(extVectorRectangle *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(extVector *))&generate_rectangle;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Changes the rectangle dimensions.

*********************************************************************************************************************/

static ERROR RECTANGLE_Resize(extVectorRectangle *Self, struct acResize *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   Self->rWidth = Args->Width;
   Self->rHeight = Args->Height;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_HEIGHT">The #Height value is a fixed coordinate.</>
<type name="FIXED_WIDTH">The #Width value is a fixed coordinate.</>
<type name="FIXED_X">The #X value is a fixed coordinate.</>
<type name="FIXED_Y">The #Y value is a fixed coordinate.</>
<type name="FIXED_RADIUS_X">The #RoundX value is a fixed coordinate.</>
<type name="FIXED_RADIUS_Y">The #RoundY value is a fixed coordinate.</>
<type name="RELATIVE_HEIGHT">The #Height value is a relative coordinate.</>
<type name="RELATIVE_WIDTH">The #Width value is a relative coordinate.</>
<type name="RELATIVE_X">The #X value is a relative coordinate.</>
<type name="RELATIVE_Y">The #Y value is a relative coordinate.</>
<type name="RELATIVE_RADIUS_X">The #RoundX value is a relative coordinate.</>
<type name="RELATIVE_RADIUS_Y">The #RoundY value is a relative coordinate.</>
</types>

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_Dimensions(extVectorRectangle *Self, LONG *Value)
{
   *Value = Self->rDimensions;
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Dimensions(extVectorRectangle *Self, LONG Value)
{
   Self->rDimensions = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Height: The height of the rectangle.  Can be expressed as a fixed or relative coordinate.

The height of the rectangle is defined here as either a fixed or relative value.  Negative values are permitted (this
will flip the rectangle on the vertical axis).

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_Height(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rHeight;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Height(extVectorRectangle *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_SCALE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_HEIGHT) & (~DMF_FIXED_HEIGHT);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);

   Self->rHeight = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
RoundX: Specifies the size of rounded corners on the horizontal axis.

The corners of a rectangle can be rounded by setting the RoundX and RoundY values.  Each value is interpreted as a
radius along the relevant axis.  A value of zero (the default) turns off this feature.

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_RoundX(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rRoundX;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_RoundX(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if ((val < 0) or (val > 1000)) return ERR_OutOfRange;

   if (Value->Type & FD_SCALE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_RADIUS_X) & (~DMF_FIXED_RADIUS_X);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_RADIUS_X) & (~DMF_RELATIVE_RADIUS_X);

   Self->rRoundX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
RoundY: Specifies the size of rounded corners on the vertical axis.

The corners of a rectangle can be rounded by setting the RoundX and RoundY values.  Each value is interpreted as a
radius along the relevant axis.  A value of zero (the default) turns off this feature.

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_RoundY(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rRoundY;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_RoundY(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if ((val < 0) or (val > 1000)) return ERR_OutOfRange;

   if (Value->Type & FD_SCALE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_RADIUS_Y) & (~DMF_FIXED_RADIUS_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_RADIUS_Y) & (~DMF_RELATIVE_RADIUS_Y);

   Self->rRoundY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
X: The left-side of the rectangle.  Can be expressed as a fixed or relative coordinate.

The position of the rectangle on the x-axis is defined here as a fixed or relative coordinate.

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_X(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rX;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_X(extVectorRectangle *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_SCALE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_X) & (~DMF_FIXED_X);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);

   Self->rX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Width: The width of the rectangle.  Can be expressed as a fixed or relative coordinate.

The width of the rectangle is defined here as either a fixed or relative value.  Negative values are permitted (this
will flip the rectangle on the horizontal axis).

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_Width(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rWidth;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Width(extVectorRectangle *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_SCALE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_WIDTH) & (~DMF_FIXED_WIDTH);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);

   Self->rWidth = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Y: The top of the rectangle.  Can be expressed as a fixed or relative coordinate.

The position of the rectangle on the y-axis is defined here as a fixed or relative coordinate.
-END-

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_Y(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rY;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Y(extVectorRectangle *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_SCALE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_Y) & (~DMF_FIXED_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);

   Self->rY = val;
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static const FieldDef clRectDimensions[] = {
   { "FixedHeight",     DMF_FIXED_HEIGHT },
   { "FixedWidth",      DMF_FIXED_WIDTH },
   { "FixedX",          DMF_FIXED_X },
   { "FixedY",          DMF_FIXED_Y },
   { "RelativeHeight",  DMF_RELATIVE_HEIGHT },
   { "RelativeWidth",   DMF_RELATIVE_WIDTH },
   { "RelativeX",       DMF_RELATIVE_X },
   { "RelativeY",       DMF_RELATIVE_Y },
   { NULL, 0 }
};

static const FieldArray clRectangleFields[] = {
   { "RoundX",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALE|FDF_RW, RECTANGLE_GET_RoundX, RECTANGLE_SET_RoundX },
   { "RoundY",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALE|FDF_RW, RECTANGLE_GET_RoundY, RECTANGLE_SET_RoundY },
   { "X",          FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALE|FDF_RW, RECTANGLE_GET_X, RECTANGLE_SET_X },
   { "Y",          FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALE|FDF_RW, RECTANGLE_GET_Y, RECTANGLE_SET_Y },
   { "Width",      FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALE|FDF_RW, RECTANGLE_GET_Width, RECTANGLE_SET_Width },
   { "Height",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALE|FDF_RW, RECTANGLE_GET_Height, RECTANGLE_SET_Height },
   { "Dimensions", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, RECTANGLE_GET_Dimensions, RECTANGLE_SET_Dimensions, &clRectDimensions },
   END_FIELD
};

static const ActionArray clRectangleActions[] = {
   { AC_Move,          RECTANGLE_Move },
   { AC_MoveToPoint,   RECTANGLE_MoveToPoint },
   { AC_NewObject,     RECTANGLE_NewObject },
   //{ AC_Redimension, RECTANGLE_Redimension },
   { AC_Resize,      RECTANGLE_Resize },
   { 0, NULL }
};

static ERROR init_rectangle(void)
{
   clVectorRectangle = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::ClassID(ID_VECTORRECTANGLE),
      fl::Name("VectorRectangle"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clRectangleActions),
      fl::Fields(clRectangleFields),
      fl::Size(sizeof(extVectorRectangle)),
      fl::Path(MOD_PATH));

   return clVectorRectangle ? ERR_Okay : ERR_AddClass;
}
