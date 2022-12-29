/*****************************************************************************

-CLASS-
VectorRectangle: Extends the Vector class with support for generating rectangles.

VectorRectangle extends the @Vector class with the ability to generate rectangular paths.

-END-

*****************************************************************************/

#include "agg_rounded_rect.h"

static void generate_rectangle(objVectorRectangle *Vector)
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

   if ((Vector->rRoundX) or (Vector->rRoundY)) {
      agg::rounded_rect aggrect(x, y, x + width, y + height, Vector->rRoundX);
      if (Vector->rRoundX != Vector->rRoundY) aggrect.radius(Vector->rRoundX, Vector->rRoundY);
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

/*****************************************************************************
-ACTION-
Move: Moves the vector to a new position.

*****************************************************************************/

static ERROR RECTANGLE_Move(objVectorRectangle *Self, struct acMove *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   Self->rX += Args->DeltaX;
   Self->rY += Args->DeltaY;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Moves the vector to a new fixed position.

*****************************************************************************/

static ERROR RECTANGLE_MoveToPoint(objVectorRectangle *Self, struct acMoveToPoint *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->Flags & MTF_X) Self->rX = Args->X;
   if (Args->Flags & MTF_Y) Self->rY = Args->Y;
   if (Args->Flags & MTF_RELATIVE) Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_X | DMF_RELATIVE_Y) & ~(DMF_FIXED_X | DMF_FIXED_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X | DMF_FIXED_Y) & ~(DMF_RELATIVE_X | DMF_RELATIVE_Y);
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static ERROR RECTANGLE_NewObject(objVectorRectangle *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(extVector *))&generate_rectangle;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Resize: Changes the rectangle dimensions.

*****************************************************************************/

static ERROR RECTANGLE_Resize(objVectorRectangle *Self, struct acResize *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   Self->rWidth = Args->Width;
   Self->rHeight = Args->Height;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_HEIGHT">The #Height value is a fixed coordinate.</>
<type name="FIXED_WIDTH">The #Width value is a fixed coordinate.</>
<type name="FIXED_X">The #X value is a fixed coordinate.</>
<type name="FIXED_Y">The #Y value is a fixed coordinate.</>
<type name="RELATIVE_HEIGHT">The #Height value is a relative coordinate.</>
<type name="RELATIVE_WIDTH">The #Width value is a relative coordinate.</>
<type name="RELATIVE_X">The #X value is a relative coordinate.</>
<type name="RELATIVE_Y">The #Y value is a relative coordinate.</>
</types>

*****************************************************************************/

static ERROR RECTANGLE_GET_Dimensions(objVectorRectangle *Self, LONG *Value)
{
   *Value = Self->rDimensions;
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Dimensions(objVectorRectangle *Self, LONG Value)
{
   Self->rDimensions = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Height: The height of the rectangle.  Can be expressed as a fixed or relative coordinate.

The height of the rectangle is defined here as either a fixed or relative value.  Negative values are permitted (this
will flip the rectangle on the vertical axis).

*****************************************************************************/

static ERROR RECTANGLE_GET_Height(objVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rHeight;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Height(objVectorRectangle *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_HEIGHT) & (~DMF_FIXED_HEIGHT);
   }
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);

   Self->rHeight = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RoundX: Specifies the size of rounded corners on the horizontal axis.

The corners of a rectangle can be rounded by setting the RoundX and RoundY values.  Each value is interpreted as a
radius along the relevant axis.  A value of zero (the default) turns off this feature.

*****************************************************************************/

static ERROR RECTANGLE_GET_RoundX(objVectorRectangle *Self, DOUBLE *Value)
{
   *Value = Self->rRoundX;
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_RoundX(objVectorRectangle *Self, DOUBLE Value)
{
   if ((Value < 0) or (Value > 1000)) return ERR_OutOfRange;
   Self->rRoundX = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RoundY: Specifies the size of rounded corners on the vertical axis.

The corners of a rectangle can be rounded by setting the RoundX and RoundY values.  Each value is interpreted as a
radius along the relevant axis.  A value of zero (the default) turns off this feature.

*****************************************************************************/

static ERROR RECTANGLE_GET_RoundY(objVectorRectangle *Self, DOUBLE *Value)
{
   *Value = Self->rRoundY;
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_RoundY(objVectorRectangle *Self, DOUBLE Value)
{
   if ((Value < 0) or (Value > 1000)) return ERR_OutOfRange;
   Self->rRoundY = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
X: The left-side of the rectangle.  Can be expressed as a fixed or relative coordinate.

The position of the rectangle on the x-axis is defined here as a fixed or relative coordinate.

*****************************************************************************/

static ERROR RECTANGLE_GET_X(objVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rX;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_X(objVectorRectangle *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_X) & (~DMF_FIXED_X);
   }
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);

   Self->rX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Width: The width of the rectangle.  Can be expressed as a fixed or relative coordinate.

The width of the rectangle is defined here as either a fixed or relative value.  Negative values are permitted (this
will flip the rectangle on the horizontal axis).

*****************************************************************************/

static ERROR RECTANGLE_GET_Width(objVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rWidth;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Width(objVectorRectangle *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_WIDTH) & (~DMF_FIXED_WIDTH);
   }
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);

   Self->rWidth = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Y: The top of the rectangle.  Can be expressed as a fixed or relative coordinate.

The position of the rectangle on the y-axis is defined here as a fixed or relative coordinate.
-END-

*****************************************************************************/

static ERROR RECTANGLE_GET_Y(objVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rY;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Y(objVectorRectangle *Self, Variable *Value)
{
   parasol::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->rDimensions = (Self->rDimensions | DMF_RELATIVE_Y) & (~DMF_FIXED_Y);
   }
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);

   Self->rY = val;
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

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
   { "RoundX",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)RECTANGLE_GET_RoundX, (APTR)RECTANGLE_SET_RoundX },
   { "RoundY",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, 0, (APTR)RECTANGLE_GET_RoundY, (APTR)RECTANGLE_SET_RoundY },
   { "X",          FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)RECTANGLE_GET_X, (APTR)RECTANGLE_SET_X },
   { "Y",          FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)RECTANGLE_GET_Y, (APTR)RECTANGLE_SET_Y },
   { "Width",      FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)RECTANGLE_GET_Width, (APTR)RECTANGLE_SET_Width },
   { "Height",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)RECTANGLE_GET_Height, (APTR)RECTANGLE_SET_Height },
   { "Dimensions", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, (MAXINT)&clRectDimensions, (APTR)RECTANGLE_GET_Dimensions, (APTR)RECTANGLE_SET_Dimensions },
   END_FIELD
};

static const ActionArray clRectangleActions[] = {
   { AC_Move,          (APTR)RECTANGLE_Move },
   { AC_MoveToPoint,   (APTR)RECTANGLE_MoveToPoint },
   { AC_NewObject,     (APTR)RECTANGLE_NewObject },
   //{ AC_Redimension, (APTR)RECTANGLE_Redimension },
   { AC_Resize,      (APTR)RECTANGLE_Resize },
   { 0, NULL }
};

static ERROR init_rectangle(void)
{
   clVectorRectangle = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::SubClassID(ID_VECTORRECTANGLE),
      fl::Name("VectorRectangle"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clRectangleActions),
      fl::Fields(clRectangleFields),
      fl::Size(sizeof(objVectorRectangle)),
      fl::Path(MOD_PATH));

   return clVectorRectangle ? ERR_Okay : ERR_AddClass;
}
