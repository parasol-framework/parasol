/*****************************************************************************

-CLASS-
VectorEllipse: Extends the Vector class with support for elliptical path generation.

The VectorEllipse class provides the necessary functionality for elliptical path generation.

-END-

*****************************************************************************/

typedef struct rkVectorEllipse {
   OBJECT_HEADER
   SHAPE_PUBLIC
   SHAPE_PRIVATE
   DOUBLE CX, CY;
   DOUBLE RadiusX, RadiusY;
   LONG Dimensions;
   LONG Vertices;
} objVectorEllipse;

//****************************************************************************

static void generate_ellipse(objVectorEllipse *Vector)
{
   DOUBLE rx = Vector->RadiusX, ry = Vector->RadiusY;

   if (Vector->Dimensions & DMF_RELATIVE_RADIUS_X) {
      if (Vector->ParentView->vpDimensions & DMF_WIDTH) rx *= Vector->ParentView->vpFixedWidth;
      else if (Vector->ParentView->vpViewWidth > 0) rx *= Vector->ParentView->vpViewWidth;
      else rx *= Vector->Scene->PageWidth;
   }

   if (Vector->Dimensions & DMF_RELATIVE_RADIUS_Y) {
      if (Vector->ParentView->vpDimensions & DMF_HEIGHT) ry *= Vector->ParentView->vpFixedHeight;
      else if (Vector->ParentView->vpViewHeight > 0) ry *= Vector->ParentView->vpViewHeight;
      else ry *= Vector->Scene->PageHeight;
   }

   DOUBLE scale = 1.0;
   if (Vector->Transform) scale = Vector->Transform->scale();

   ULONG steps;
   if (Vector->Vertices >= 3) steps = Vector->Vertices;
   else {
      DOUBLE ra = (fabs(rx) + fabs(ry)) / 2.0;
      DOUBLE da = acos(ra / (ra + 0.125 / scale)) * 2.0;
      steps = agg::uround(2.0 * agg::pi / da);
      if (steps < 3) steps = 3; // Because you need at least 3 vertices to create a shape.
   }

   for (ULONG step=0; step < steps; step++) {
      DOUBLE angle = DOUBLE(step) / DOUBLE(steps) * 2.0 * agg::pi;
      //if (m_cw) angle = 2.0 * agg::pi - angle;
      DOUBLE x = rx + cos(angle) * rx;
      DOUBLE y = ry + sin(angle) * ry;
      if (step == 0) Vector->BasePath->move_to(x, y);
      else Vector->BasePath->line_to(x, y);
   }
   Vector->BasePath->close_polygon();
}

//****************************************************************************

static void get_ellipse_xy(objVectorEllipse *Vector)
{
   DOUBLE cx = Vector->CX, cy = Vector->CY;
   DOUBLE rx = Vector->RadiusX, ry = Vector->RadiusY;

   if (Vector->Dimensions & DMF_RELATIVE_CENTER_X) {
      if (Vector->ParentView->vpDimensions & DMF_WIDTH) cx *= Vector->ParentView->vpFixedWidth;
      else if (Vector->ParentView->vpViewWidth > 0) cx *= Vector->ParentView->vpViewWidth;
      else cx *= Vector->Scene->PageWidth;
   }

   if (Vector->Dimensions & DMF_RELATIVE_CENTER_Y) {
      if (Vector->ParentView->vpDimensions & DMF_HEIGHT) cy *= Vector->ParentView->vpFixedHeight;
      else if (Vector->ParentView->vpViewHeight > 0) cy *= Vector->ParentView->vpViewHeight;
      else cy *= Vector->Scene->PageHeight;
   }

   if (Vector->Dimensions & DMF_RELATIVE_RADIUS_X) {
      if (Vector->ParentView->vpDimensions & DMF_WIDTH) rx *= Vector->ParentView->vpFixedWidth;
      else if (Vector->ParentView->vpViewWidth > 0) rx *= Vector->ParentView->vpViewWidth;
      else rx *= Vector->Scene->PageWidth;
   }

   if (Vector->Dimensions & DMF_RELATIVE_RADIUS_Y) {
      if (Vector->ParentView->vpDimensions & DMF_HEIGHT) ry *= Vector->ParentView->vpFixedHeight;
      else if (Vector->ParentView->vpViewHeight > 0) ry *= Vector->ParentView->vpViewHeight;
      else ry *= Vector->Scene->PageHeight;
   }

   Vector->FinalX = cx - rx;
   Vector->FinalY = cy - ry;
}

/*****************************************************************************
-ACTION-
Move: Moves the center of the ellipse by a relative distance.

*****************************************************************************/

static ERROR ELLIPSE_Move(objVectorEllipse *Self, struct acMove *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   Self->CX += Args->XChange;
   Self->CY += Args->YChange;
   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Moves the center of the ellipse to a new position.

*****************************************************************************/

static ERROR ELLIPSE_MoveToPoint(objVectorEllipse *Self, struct acMoveToPoint *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->Flags & MTF_X) Self->CX = Args->X;
   if (Args->Flags & MTF_Y) Self->CY = Args->Y;
   if (Args->Flags & MTF_RELATIVE) Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_X | DMF_RELATIVE_CENTER_Y) & ~(DMF_FIXED_CENTER_X | DMF_FIXED_CENTER_Y);
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_X | DMF_FIXED_CENTER_Y) & ~(DMF_RELATIVE_CENTER_X | DMF_RELATIVE_CENTER_Y);
   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

//****************************************************************************

static ERROR ELLIPSE_NewObject(objVectorEllipse *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(struct rkVector *))&generate_ellipse;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_RADIUS_X">The #RadiusX value is a fixed coordinate.</>
<type name="FIXED_RADIUS_Y">The #RadiusY value is a fixed coordinate.</>
<type name="FIXED_CENTER_X">The #CenterX value is a fixed coordinate.</>
<type name="FIXED_CENTER_Y">The #CenterY value is a fixed coordinate.</>
<type name="RELATIVE_RADIUS_X">The #RadiusX value is a relative coordinate.</>
<type name="RELATIVE_RADIUS_Y">The #RadiusY value is a relative coordinate.</>
<type name="RELATIVE_CENTER_X">The #CenterX value is a relative coordinate.</>
<type name="RELATIVE_CENTER_Y">The #CenterY value is a relative coordinate.</>
</types>

*****************************************************************************/

static ERROR ELLIPSE_GET_Dimensions(objVectorEllipse *Self, LONG *Value)
{
   *Value = Self->Dimensions;
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Dimensions(objVectorEllipse *Self, LONG Value)
{
   Self->Dimensions = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Height: The height (vertical diameter) of the ellipse.

The height of the ellipse is defined here as the equivalent of #RadiusY * 2.0.

*****************************************************************************/

static ERROR ELLIPSE_GET_Height(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = Self->RadiusY * 2.0;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Height(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);
   Self->RadiusY = val * 0.5;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
CenterX: The horizontal center of the ellipse.  Expressed as a fixed or relative coordinate.

The horizontal center of the ellipse is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR ELLIPSE_GET_CenterX(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = Self->CX;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Dimensions & DMF_RELATIVE_CENTER_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_CenterX(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_X) & (~DMF_FIXED_CENTER_X);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_X) & (~DMF_RELATIVE_CENTER_X);

   Self->CX = val;

   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
CenterY: The vertical center of the ellipse.  Expressed as a fixed or relative coordinate.

The vertical center of the ellipse is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR ELLIPSE_GET_CenterY(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = Self->CY;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Dimensions & DMF_RELATIVE_CENTER_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_CenterY(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_Y) & (~DMF_FIXED_CENTER_Y);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_Y) & (~DMF_RELATIVE_CENTER_Y);

   Self->CY = val;
   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Radius: The radius of the ellipse.  Expressed as a fixed or relative coordinate.

The radius of the ellipse is defined here as either a fixed or relative value.  Updating the radius will set both the
#RadiusX and #RadiusY values simultaneously.

*****************************************************************************/

static ERROR ELLIPSE_GET_Radius(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = (Self->RadiusX + Self->RadiusY) * 0.5;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Dimensions & DMF_RELATIVE_RADIUS)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Radius(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_RADIUS) & (~DMF_FIXED_RADIUS);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_RADIUS) & (~DMF_RELATIVE_RADIUS);

   Self->RadiusX = Self->RadiusY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
RadiusX: The horizontal radius of the ellipse.

The horizontal radius of the ellipse is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR ELLIPSE_GET_RadiusX(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = Self->RadiusX;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Dimensions & DMF_RELATIVE_RADIUS_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_RadiusX(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_RADIUS_X) & (~DMF_FIXED_RADIUS_X);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_RADIUS_X) & (~DMF_RELATIVE_RADIUS_X);

   Self->RadiusX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
RadiusY: The vertical radius of the ellipse.

The vertical radius of the ellipse is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR ELLIPSE_GET_RadiusY(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = Self->RadiusY;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Dimensions & DMF_RELATIVE_RADIUS_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_RadiusY(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_RADIUS_Y) & (~DMF_FIXED_RADIUS_Y);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_RADIUS_Y) & (~DMF_RELATIVE_RADIUS_Y);

   Self->RadiusY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Vertices: Limits the total number of vertices generated for the ellipse.

Setting a value in Vertices will limit the total number of vertices that are generated for the ellipse.  This feature
is useful for generating common convex geometrical shapes such as triangles, polygons, hexagons and so forth; because
their vertices will always touch the sides of an elliptical area.

Please note that this feature is not part of the SVG standard.

*****************************************************************************/

static ERROR ELLIPSE_GET_Vertices(objVectorEllipse *Self, LONG *Value)
{
   *Value = Self->Vertices;
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Vertices(objVectorEllipse *Self, LONG Value)
{
   if (((Value >= 3) AND (Value < 4096)) OR (!Value)) {
      Self->Vertices = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return PostError(ERR_InvalidValue);
}

/*****************************************************************************
-FIELD-
Width: The width (horizontal diameter) of the ellipse.

The width of the ellipse is defined here as the equivalent of #RadiusX * 2.0.
-END-
*****************************************************************************/

static ERROR ELLIPSE_GET_Width(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val = Self->RadiusX * 2.0;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Width(objVectorEllipse *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);
   Self->RadiusX = val * 0.5;
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static const struct FieldDef clEllipseDimensions[] = {
   { "FixedRadiusX",    DMF_FIXED_RADIUS_X },
   { "FixedRadiusY",    DMF_FIXED_RADIUS_Y },
   { "FixedCenterX",    DMF_FIXED_CENTER_X },
   { "FixedCenterY",    DMF_FIXED_CENTER_Y },
   { "RelativeRadiusX", DMF_RELATIVE_RADIUS_X },
   { "RelativeRadiusY", DMF_RELATIVE_RADIUS_Y },
   { "RelativeCenterX", DMF_RELATIVE_CENTER_X },
   { "RelativeCenterY", DMF_RELATIVE_CENTER_Y },
   { NULL, 0 }
};

static const struct FieldArray clEllipseFields[] = {
   { "Width",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_Width,   (APTR)ELLIPSE_SET_Width },
   { "Height",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_Height,  (APTR)ELLIPSE_SET_Height },
   { "CenterX",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_CenterX, (APTR)ELLIPSE_SET_CenterX },
   { "CenterY",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_CenterY, (APTR)ELLIPSE_SET_CenterY },
   { "Radius",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_Radius,  (APTR)ELLIPSE_SET_Radius },
   { "RadiusX",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_RadiusX, (APTR)ELLIPSE_SET_RadiusX },
   { "RadiusY",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_RadiusY, (APTR)ELLIPSE_SET_RadiusY },
   { "Dimensions", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, (MAXINT)&clEllipseDimensions, (APTR)ELLIPSE_GET_Dimensions, (APTR)ELLIPSE_SET_Dimensions },
   { "Vertices",   FDF_VIRTUAL|FDF_LONG|FDF_RW, 0, (APTR)ELLIPSE_GET_Vertices, (APTR)ELLIPSE_SET_Vertices },
   // Synonyms
   { "CX", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_CenterX, (APTR)ELLIPSE_SET_CenterX },
   { "CY", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_CenterY, (APTR)ELLIPSE_SET_CenterY },
   { "R",  FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_Radius,  (APTR)ELLIPSE_SET_Radius },
   { "RX", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_RadiusX, (APTR)ELLIPSE_SET_RadiusX },
   { "RY", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)ELLIPSE_GET_RadiusY, (APTR)ELLIPSE_SET_RadiusY },
   END_FIELD
};

static const struct ActionArray clEllipseActions[] = {
   { AC_NewObject,     (APTR)ELLIPSE_NewObject },
   { AC_Move,          (APTR)ELLIPSE_Move },
   { AC_MoveToPoint,   (APTR)ELLIPSE_MoveToPoint },
   //{ AC_Redimension, (APTR)ELLIPSE_Redimension },
   //{ AC_Resize,      (APTR)ELLIPSE_Resize },
   { 0, NULL }
};

static ERROR init_ellipse(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorEllipse,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORELLIPSE,
      FID_Name|TSTRING,      "VectorEllipse",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clEllipseActions,
      FID_Fields|TARRAY,     clEllipseFields,
      FID_Size|TLONG,        sizeof(objVectorEllipse),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
