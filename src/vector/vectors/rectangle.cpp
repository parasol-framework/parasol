/*********************************************************************************************************************

-CLASS-
VectorRectangle: Extends the Vector class with support for generating rectangles.

VectorRectangle extends the @Vector class with the ability to generate rectangular paths.

-END-

*********************************************************************************************************************/

#include "agg_rounded_rect.h"

static void generate_rectangle(extVectorRectangle *Vector)
{
   DOUBLE x, y, width, height;

   if (Vector->rDimensions & DMF_FIXED_X) x = Vector->rX;
   else if (Vector->rDimensions & DMF_SCALED_X) x = Vector->rX * get_parent_width(Vector);
   else if ((Vector->rDimensions & DMF_WIDTH) and (Vector->rDimensions & DMF_X_OFFSET)) {
      if (Vector->rDimensions & DMF_FIXED_WIDTH) width = Vector->rWidth;
      else width = get_parent_width(Vector) * Vector->rWidth;

      if (Vector->rDimensions & DMF_FIXED_X_OFFSET) x = get_parent_width(Vector) - width - Vector->rXOffset;
      else x = get_parent_width(Vector) - width - (get_parent_width(Vector) * Vector->rXOffset);
   }
   else x = 0;

   if (Vector->rDimensions & DMF_FIXED_Y) y = Vector->rY;
   else if (Vector->rDimensions & DMF_SCALED_Y) y = Vector->rY * get_parent_height(Vector);
   else if ((Vector->rDimensions & DMF_WIDTH) and (Vector->rDimensions & DMF_Y_OFFSET)) {
      if (Vector->rDimensions & DMF_FIXED_WIDTH) height = Vector->rHeight;
      else height = get_parent_height(Vector) * Vector->rHeight;

      if (Vector->rDimensions & DMF_FIXED_Y_OFFSET) y = get_parent_height(Vector) - height - Vector->rYOffset;
      else y = get_parent_height(Vector) - height - (get_parent_height(Vector) * Vector->rYOffset);
   }
   else y = 0;

   if (Vector->rDimensions & DMF_FIXED_WIDTH) width = Vector->rWidth;
   else if (Vector->rDimensions & DMF_SCALED_WIDTH) width = Vector->rWidth * get_parent_width(Vector);
   else if (Vector->rDimensions & (DMF_FIXED_X_OFFSET|DMF_SCALED_X_OFFSET)) {
      if (Vector->rDimensions & DMF_SCALED_X) x = Vector->rX * get_parent_width(Vector);
      else x = Vector->rX;

      if (Vector->rDimensions & DMF_FIXED_X_OFFSET) width = get_parent_width(Vector) - Vector->rXOffset - x;
      else width = get_parent_width(Vector) - (Vector->rXOffset * get_parent_width(Vector)) - x;
   }
   else width = get_parent_width(Vector);

   if (Vector->rDimensions & DMF_FIXED_HEIGHT) height = Vector->rHeight;
   else if (Vector->rDimensions & DMF_SCALED_HEIGHT) height = Vector->rHeight * get_parent_height(Vector);
   else if (Vector->rDimensions & (DMF_FIXED_Y_OFFSET|DMF_SCALED_Y_OFFSET)) {
      if (Vector->rDimensions & DMF_SCALED_Y) y = Vector->rY * get_parent_height(Vector);
      else y = Vector->rY;

      if (Vector->rDimensions & DMF_FIXED_Y_OFFSET) height = get_parent_height(Vector) - Vector->rYOffset - y;
      else height = get_parent_height(Vector) - (Vector->rYOffset * get_parent_height(Vector)) - y;
   }
   else height = get_parent_height(Vector);

   if (Vector->rFullControl) {
      // Full control of rounded corners has been requested by the client (four X,Y coordinate pairs).
      // Coordinates are either ALL scaled or ALL fixed, not a mix of both.
      // This feature is not SVG compliant.

      DOUBLE scale_x = 1.0, scale_y = 1.0;

      if (Vector->rDimensions & DMF_SCALED_RADIUS_X) {
         scale_x = sqrt((width * width) + (height * height)) * INV_SQRT2;
      }

      if (Vector->rDimensions & DMF_SCALED_RADIUS_Y) {
         if (scale_x != 1.0) scale_x = scale_x;
         else scale_y = sqrt((width * width) + (height * height)) * INV_SQRT2;
      }

      DOUBLE rx[4], ry[4];
      for (unsigned i=0; i < 4; i++) {
         rx[i] = Vector->rRound[i].x * scale_x;
         ry[i] = Vector->rRound[i].y * scale_y;
         if (rx[i] > width * 0.5) rx[i] = width * 0.5;
         if (ry[i] > height * 0.5) ry[i] = height * 0.5;
      }

      agg::rounded_rect aggrect(x, y, x + width, y + height, 0, 0);
      aggrect.approximation_scale(Vector->Transform.scale());
      aggrect.radius(rx[0], ry[0], rx[1], ry[1], rx[2], ry[2], rx[3], ry[3]);
      aggrect.normalize_radius(); // Required because???

      Vector->BasePath.concat_path(aggrect);
   }
   else if (Vector->rRound[0].x > 0) {
      // SVG rules that RX will also apply to RY unless RY != 0.
      // An RX of zero disables rounding (contrary to SVG).
      // If RX is greater than width/2, set RX to width/2.  Same for RY on the vertical axis.

      DOUBLE rx = Vector->rRound[0].x, ry = Vector->rRound[0].y;

      if (Vector->rDimensions & DMF_SCALED_RADIUS_X) {
         rx *= sqrt((width * width) + (height * height)) * INV_SQRT2;
      }

      if (rx > width * 0.5) rx = width * 0.5; // SVG rule

      if ((rx != ry) and (ry)) {
         if (Vector->rDimensions & DMF_SCALED_RADIUS_Y) {
            ry *= sqrt((width * width) + (height * height)) * INV_SQRT2;
         }
         if (ry > height * 0.5) ry = height * 0.5;
      }
      else ry = rx;

      agg::rounded_rect aggrect(x, y, x + width, y + height, rx, ry);
      aggrect.approximation_scale(Vector->Transform.scale());
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

   Vector->Bounds = { x, y, x + width, y + height };
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
   if ((Args->Flags & MTF::RELATIVE) != MTF::NIL) Self->rDimensions = (Self->rDimensions | DMF_SCALED_X | DMF_SCALED_Y) & ~(DMF_FIXED_X | DMF_FIXED_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X | DMF_FIXED_Y) & ~(DMF_SCALED_X | DMF_SCALED_Y);
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
Dimensions: Dimension flags define whether individual dimension fields contain fixed or scaled values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_HEIGHT">The #Height value is a fixed coordinate.</>
<type name="FIXED_WIDTH">The #Width value is a fixed coordinate.</>
<type name="FIXED_X">The #X value is a fixed coordinate.</>
<type name="FIXED_Y">The #Y value is a fixed coordinate.</>
<type name="FIXED_RADIUS_X">The #RoundX value is a fixed coordinate.</>
<type name="FIXED_RADIUS_Y">The #RoundY value is a fixed coordinate.</>
<type name="SCALED_HEIGHT">The #Height value is a scaled coordinate.</>
<type name="SCALED_WIDTH">The #Width value is a scaled coordinate.</>
<type name="SCALED_X">The #X value is a scaled coordinate.</>
<type name="SCALED_Y">The #Y value is a scaled coordinate.</>
<type name="SCALED_RADIUS_X">The #RoundX value is a scaled coordinate.</>
<type name="SCALED_RADIUS_Y">The #RoundY value is a scaled coordinate.</>
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
Height: The height of the rectangle.  Can be expressed as a fixed or scaled coordinate.

The height of the rectangle is defined here as either a fixed or scaled value.  Negative values are permitted (this
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

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_HEIGHT) & (~DMF_FIXED_HEIGHT);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_HEIGHT) & (~DMF_SCALED_HEIGHT);

   Self->rHeight = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Rounding: Precisely controls rounded corner positioning.

Set the Rounding field if all four corners of the rectangle need to be precisely controlled.  Four X,Y sizing
pairs must be provided in sequence, with the first describing the top-left corner and proceeding in clockwise fashion.
Each pair of values is equivalent to a #RoundX,#RoundY definition for that corner only.

By default, values will be treated as fixed pixel units.  They can be changed to scaled values by defining the
`DMF_SCALED_RADIUS_X` and/or `DMF_SCALED_RADIUS_Y` flags in the #Dimensions field.  The scale is calculated
against the rectangle's diagonal.

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_Rounding(extVectorRectangle *Self, DOUBLE **Value, LONG *Elements)
{
   *Value = (DOUBLE *)Self->rRound.data();
   *Elements = 8;
   return ERR_Okay;
}

static ERROR RECTANGLE_SET_Rounding(extVectorRectangle *Self, DOUBLE *Value, LONG Elements)
{
   if (Elements >= 8) {
      CopyMemory(Value, Self->rRound.data(), sizeof(DOUBLE) * 8);
      Self->rFullControl = true;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************

-FIELD-
RoundX: Specifies the size of rounded corners on the horizontal axis.

The corners of a rectangle can be rounded by setting the RoundX and RoundY values.  Each value is interpreted as a
radius along the relevant axis.  A value of zero (the default) turns off this feature.

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_RoundX(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE val = Self->rRound[0].x;
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

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_RADIUS_X) & (~DMF_FIXED_RADIUS_X);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_RADIUS_X) & (~DMF_SCALED_RADIUS_X);

   Self->rRound[0].x = Self->rRound[1].x = Self->rRound[2].x = Self->rRound[3].x = val;
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
   DOUBLE val = Self->rRound[0].y;
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

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_RADIUS_Y) & (~DMF_FIXED_RADIUS_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_RADIUS_Y) & (~DMF_SCALED_RADIUS_Y);

   Self->rRound[0].y = Self->rRound[1].y = Self->rRound[2].y = Self->rRound[3].y = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
X: The left-side of the rectangle.  Can be expressed as a fixed or scaled coordinate.
-END-

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
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_X) & (~DMF_FIXED_X);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X) & (~DMF_SCALED_X);

   Self->rX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
XOffset: The right-side of the rectangle, expressed as a fixed or scaled offset value.
-END-

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_XOffset(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE value = 0;

   if (Self->rDimensions & DMF_FIXED_X_OFFSET) value = Self->rXOffset;
   else if (Self->rDimensions & DMF_SCALED_X_OFFSET) {
      value = Self->rXOffset * get_parent_width(Self);
   }
   else if ((Self->rDimensions & DMF_X) and (Self->rDimensions & DMF_WIDTH)) {
      DOUBLE width;
      if (Self->rDimensions & DMF_FIXED_WIDTH) width = Self->rHeight;
      else width = get_parent_width(Self) * Self->rHeight;

      if (Self->rDimensions & DMF_FIXED_X) value = get_parent_width(Self) - (Self->rX + width);
      else value = get_parent_width(Self) - ((Self->rX * get_parent_width(Self)) + width);
   }
   else value = 0;

   if (Value->Type & FD_SCALED) value = value / get_parent_width(Self);

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return ERR_FieldTypeMismatch;

   return ERR_Okay;
}

static ERROR RECTANGLE_SET_XOffset(extVectorRectangle *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->rXOffset = Value->Double;
   else if (Value->Type & FD_LARGE) Self->rXOffset = Value->Large;
   else if (Value->Type & FD_STRING) Self->rXOffset = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_X_OFFSET) & (~DMF_FIXED_X_OFFSET);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_X_OFFSET) & (~DMF_SCALED_X_OFFSET);

   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Width: The width of the rectangle.  Can be expressed as a fixed or scaled coordinate.

The width of the rectangle is defined here as either a fixed or scaled value.  Negative values are permitted (this
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
   if (Value->Type & FD_DOUBLE) Self->rWidth = Value->Double;
   else if (Value->Type & FD_LARGE) Self->rWidth = Value->Large;
   else if (Value->Type & FD_STRING) Self->rWidth = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_WIDTH) & (~DMF_FIXED_WIDTH);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_WIDTH) & (~DMF_SCALED_WIDTH);

   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Y: The top of the rectangle.  Can be expressed as a fixed or scaled coordinate.
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
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_Y) & (~DMF_FIXED_Y);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_Y) & (~DMF_SCALED_Y);

   Self->rY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
YOffset: The bottom of the rectangle, expressed as a fixed or scaled offset value.
-END-

*********************************************************************************************************************/

static ERROR RECTANGLE_GET_YOffset(extVectorRectangle *Self, Variable *Value)
{
   DOUBLE value = 0;

   if (Self->rDimensions & DMF_FIXED_Y_OFFSET) value = Self->rYOffset;
   else if (Self->rDimensions & DMF_SCALED_Y_OFFSET) {
      value = Self->rYOffset * get_parent_height(Self);
   }
   else if ((Self->rDimensions & DMF_Y) and (Self->rDimensions & DMF_HEIGHT)) {
      DOUBLE height;
      if (Self->rDimensions & DMF_FIXED_HEIGHT) height = Self->rHeight;
      else height = get_parent_height(Self) * Self->rHeight;

      if (Self->rDimensions & DMF_FIXED_Y) value = get_parent_height(Self) - (Self->rY + height);
      else value = get_parent_height(Self) - ((Self->rY * get_parent_height(Self)) + height);
   }
   else value = 0;

   if (Value->Type & FD_SCALED) value = value / get_parent_height(Self);

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(value);
   else return ERR_FieldTypeMismatch;

   return ERR_Okay;
}

static ERROR RECTANGLE_SET_YOffset(extVectorRectangle *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) Self->rYOffset = Value->Double;
   else if (Value->Type & FD_LARGE) Self->rYOffset = Value->Large;
   else if (Value->Type & FD_STRING) Self->rYOffset = strtod((CSTRING)Value->Pointer, NULL);
   else return ERR_SetValueNotNumeric;

   if (Value->Type & FD_SCALED) Self->rDimensions = (Self->rDimensions | DMF_SCALED_Y_OFFSET) & (~DMF_FIXED_Y_OFFSET);
   else Self->rDimensions = (Self->rDimensions | DMF_FIXED_Y_OFFSET) & (~DMF_SCALED_Y_OFFSET);

   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static const FieldDef clRectDimensions[] = {
   { "FixedHeight",   DMF_FIXED_HEIGHT },
   { "FixedWidth",    DMF_FIXED_WIDTH },
   { "FixedX",        DMF_FIXED_X },
   { "FixedY",        DMF_FIXED_Y },
   { "FixedXOffset",  DMF_FIXED_X_OFFSET },
   { "FixedYOffset",  DMF_FIXED_Y_OFFSET },
   { "ScaledHeight",  DMF_SCALED_HEIGHT },
   { "ScaledWidth",   DMF_SCALED_WIDTH },
   { "ScaledX",       DMF_SCALED_X },
   { "ScaledY",       DMF_SCALED_Y },
   { "ScaledXOffset", DMF_SCALED_X_OFFSET },
   { "ScaledYOffset", DMF_SCALED_Y_OFFSET },
   { NULL, 0 }
};

static const FieldArray clRectangleFields[] = {
   { "Rounding",   FDF_VIRTUAL|FDF_DOUBLE|FDF_ARRAY|FDF_RW, RECTANGLE_GET_Rounding, RECTANGLE_SET_Rounding },
   { "RoundX",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_RoundX, RECTANGLE_SET_RoundX },
   { "RoundY",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_RoundY, RECTANGLE_SET_RoundY },
   { "X",          FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_X, RECTANGLE_SET_X },
   { "Y",          FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_Y, RECTANGLE_SET_Y },
   { "XOffset",    FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_XOffset, RECTANGLE_SET_XOffset },
   { "YOffset",    FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_YOffset, RECTANGLE_SET_YOffset },
   { "Width",      FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_Width, RECTANGLE_SET_Width },
   { "Height",     FDF_VIRTUAL|FD_VARIABLE|FDF_DOUBLE|FDF_SCALED|FDF_RW, RECTANGLE_GET_Height, RECTANGLE_SET_Height },
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
