/*********************************************************************************************************************

-CLASS-
VectorEllipse: Extends the Vector class with support for elliptical path generation.

The VectorEllipse class provides the necessary functionality for elliptical path generation.

-END-

*********************************************************************************************************************/

class extVectorEllipse : public extVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORELLIPSE;
   static constexpr CSTRING CLASS_NAME = "VectorEllipse";
   using create = pf::Create<extVectorEllipse>;

   DOUBLE eCX, eCY;
   DOUBLE eRadiusX, eRadiusY;
   LONG eDimensions;
   LONG eVertices;
};

//********************************************************************************************************************

static void generate_ellipse(extVectorEllipse *Vector)
{
   DOUBLE cx = Vector->eCX, cy = Vector->eCY;
   DOUBLE rx = Vector->eRadiusX, ry = Vector->eRadiusY;

   if (Vector->eDimensions & (DMF_RELATIVE_CENTER_X|DMF_RELATIVE_CENTER_Y|DMF_RELATIVE_RADIUS_X|DMF_RELATIVE_RADIUS_Y)) {
      DOUBLE view_width, view_height;
      get_parent_size(Vector, view_width, view_height);

      if (Vector->eDimensions & DMF_RELATIVE_CENTER_X) cx *= view_width;
      if (Vector->eDimensions & DMF_RELATIVE_CENTER_Y) cy *= view_height;
      if (Vector->eDimensions & DMF_RELATIVE_RADIUS_X) rx *= view_width;
      if (Vector->eDimensions & DMF_RELATIVE_RADIUS_Y) ry *= view_height;
   }

   DOUBLE scale = Vector->Transform.scale();

   ULONG steps;
   if (Vector->eVertices >= 3) steps = Vector->eVertices;
   else {
      DOUBLE ra = (fabs(rx) + fabs(ry)) * 0.5;
      DOUBLE da = acos(ra / (ra + 0.125 / scale)) * 2.0;
      steps = agg::uround(2.0 * agg::pi / da);
      if (steps < 3) steps = 3; // Because you need at least 3 vertices to create a shape.
   }

   // TODO: Using a cosine lookup table would speed up this loop.

   for (ULONG step=0; step < steps; step++) {
      DOUBLE angle = DOUBLE(step) / DOUBLE(steps) * 2.0 * agg::pi;
      //if (m_cw) angle = 2.0 * agg::pi - angle;
      DOUBLE x = cx + cos(angle) * rx;
      DOUBLE y = cy + sin(angle) * ry;
      if (step == 0) Vector->BasePath.move_to(x, y);
      else Vector->BasePath.line_to(x, y);
   }
   Vector->BasePath.close_polygon();

   Vector->BX1 = cx - rx;
   Vector->BY1 = cy - ry;
   Vector->BX2 = cx + rx;
   Vector->BY2 = cy + ry;
}

/*********************************************************************************************************************
-ACTION-
Move: Moves the center of the ellipse by a relative distance.

*********************************************************************************************************************/

static ERROR ELLIPSE_Move(extVectorEllipse *Self, struct acMove *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->eCX += Args->DeltaX;
   Self->eCY += Args->DeltaY;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Moves the center of the ellipse to a new position.

*********************************************************************************************************************/

static ERROR ELLIPSE_MoveToPoint(extVectorEllipse *Self, struct acMoveToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Flags & MTF_X) Self->eCX = Args->X;
   if (Args->Flags & MTF_Y) Self->eCY = Args->Y;
   if (Args->Flags & MTF_RELATIVE) Self->eDimensions = (Self->eDimensions | DMF_RELATIVE_CENTER_X | DMF_RELATIVE_CENTER_Y) & ~(DMF_FIXED_CENTER_X | DMF_FIXED_CENTER_Y);
   else Self->eDimensions = (Self->eDimensions | DMF_FIXED_CENTER_X | DMF_FIXED_CENTER_Y) & ~(DMF_RELATIVE_CENTER_X | DMF_RELATIVE_CENTER_Y);
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR ELLIPSE_NewObject(extVectorEllipse *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(extVector *))&generate_ellipse;
   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_Dimensions(extVectorEllipse *Self, LONG *Value)
{
   *Value = Self->eDimensions;
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Dimensions(extVectorEllipse *Self, LONG Value)
{
   Self->eDimensions = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Height: The height (vertical diameter) of the ellipse.

The height of the ellipse is defined here as the equivalent of #RadiusY * 2.0.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_Height(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = Self->eRadiusY * 2.0;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Height(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;
   Self->eRadiusY = val * 0.5;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterX: The horizontal center of the ellipse.  Expressed as a fixed or relative coordinate.

The horizontal center of the ellipse is defined here as either a fixed or relative value.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_CenterX(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = Self->eCX;
   if ((Value->Type & FD_PERCENTAGE) and (Self->eDimensions & DMF_RELATIVE_CENTER_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_CenterX(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->eDimensions = (Self->eDimensions | DMF_RELATIVE_CENTER_X) & (~DMF_FIXED_CENTER_X);
   }
   else Self->eDimensions = (Self->eDimensions | DMF_FIXED_CENTER_X) & (~DMF_RELATIVE_CENTER_X);

   Self->eCX = val;

   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterY: The vertical center of the ellipse.  Expressed as a fixed or relative coordinate.

The vertical center of the ellipse is defined here as either a fixed or relative value.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_CenterY(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = Self->eCY;
   if ((Value->Type & FD_PERCENTAGE) and (Self->eDimensions & DMF_RELATIVE_CENTER_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_CenterY(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->eDimensions = (Self->eDimensions | DMF_RELATIVE_CENTER_Y) & (~DMF_FIXED_CENTER_Y);
   }
   else Self->eDimensions = (Self->eDimensions | DMF_FIXED_CENTER_Y) & (~DMF_RELATIVE_CENTER_Y);

   Self->eCY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Radius: The radius of the ellipse.  Expressed as a fixed or relative coordinate.

The radius of the ellipse is defined here as either a fixed or relative value.  Updating the radius will set both the
#RadiusX and #RadiusY values simultaneously.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_Radius(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = (Self->eRadiusX + Self->eRadiusY) * 0.5;
   if ((Value->Type & FD_PERCENTAGE) and (Self->eDimensions & DMF_RELATIVE_RADIUS)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Radius(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->eDimensions = (Self->eDimensions | DMF_RELATIVE_RADIUS) & (~DMF_FIXED_RADIUS);
   }
   else Self->eDimensions = (Self->eDimensions | DMF_FIXED_RADIUS) & (~DMF_RELATIVE_RADIUS);

   Self->eRadiusX = Self->eRadiusY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
RadiusX: The horizontal radius of the ellipse.

The horizontal radius of the ellipse is defined here as either a fixed or relative value.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_RadiusX(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = Self->eRadiusX;
   if ((Value->Type & FD_PERCENTAGE) and (Self->eDimensions & DMF_RELATIVE_RADIUS_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_RadiusX(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->eDimensions = (Self->eDimensions | DMF_RELATIVE_RADIUS_X) & (~DMF_FIXED_RADIUS_X);
   }
   else Self->eDimensions = (Self->eDimensions | DMF_FIXED_RADIUS_X) & (~DMF_RELATIVE_RADIUS_X);

   Self->eRadiusX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
RadiusY: The vertical radius of the ellipse.

The vertical radius of the ellipse is defined here as either a fixed or relative value.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_RadiusY(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = Self->eRadiusY;
   if ((Value->Type & FD_PERCENTAGE) and (Self->eDimensions & DMF_RELATIVE_RADIUS_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_RadiusY(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->eDimensions = (Self->eDimensions | DMF_RELATIVE_RADIUS_Y) & (~DMF_FIXED_RADIUS_Y);
   }
   else Self->eDimensions = (Self->eDimensions | DMF_FIXED_RADIUS_Y) & (~DMF_RELATIVE_RADIUS_Y);

   Self->eRadiusY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Vertices: Limits the total number of vertices generated for the ellipse.

Setting a value in Vertices will limit the total number of vertices that are generated for the ellipse.  This feature
is useful for generating common convex geometrical shapes such as triangles, polygons, hexagons and so forth; because
their vertices will always touch the sides of an elliptical area.

Please note that this feature is not part of the SVG standard.

*********************************************************************************************************************/

static ERROR ELLIPSE_GET_Vertices(extVectorEllipse *Self, LONG *Value)
{
   *Value = Self->eVertices;
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Vertices(extVectorEllipse *Self, LONG Value)
{
   if (((Value >= 3) and (Value < 4096)) or (!Value)) {
      Self->eVertices = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Width: The width (horizontal diameter) of the ellipse.

The width of the ellipse is defined here as the equivalent of #RadiusX * 2.0.
-END-
*********************************************************************************************************************/

static ERROR ELLIPSE_GET_Width(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val = Self->eRadiusX * 2.0;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR ELLIPSE_SET_Width(extVectorEllipse *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;
   Self->eRadiusX = val * 0.5;
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static const FieldDef clEllipseDimensions[] = {
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

static const FieldArray clEllipseFields[] = {
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

static const ActionArray clEllipseActions[] = {
   { AC_NewObject,     (APTR)ELLIPSE_NewObject },
   { AC_Move,          (APTR)ELLIPSE_Move },
   { AC_MoveToPoint,   (APTR)ELLIPSE_MoveToPoint },
   //{ AC_Redimension, (APTR)ELLIPSE_Redimension },
   //{ AC_Resize,      (APTR)ELLIPSE_Resize },
   { 0, NULL }
};

static ERROR init_ellipse(void)
{
   clVectorEllipse = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::SubClassID(ID_VECTORELLIPSE),
      fl::Name("VectorEllipse"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clEllipseActions),
      fl::Fields(clEllipseFields),
      fl::Size(sizeof(extVectorEllipse)),
      fl::Path(MOD_PATH));

   return clVectorEllipse ? ERR_Okay : ERR_AddClass;
}
