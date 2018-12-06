/*****************************************************************************

-CLASS-
VectorPolygon: Extends the Vector class with support for generating polygons.

The VectorPolygon class provides support for three different types of vector:

* Closed-point polygons consisting of at least 3 points.
* Open polygons consisting of at least 3 points (a 'polyline' in SVG).
* Single lines consisting of two points only (a 'line' in SVG).

To create a polyline, set the #Closed field to FALSE (defaults to TRUE).  To create a line, set the Closed
field to FALSE and set only two points (#X1,#Y1) and (#X2,#Y2)

-END-

TODO: Add a SetPoint(DOUBLE X, DOUBLE Y) method for modifying existing points.

*****************************************************************************/

#define MAX_POINTS 1024 * 16 // Maximum of 16k points per polygon object.

static void generate_polygon(objVectorPoly *Vector)
{
   DOUBLE view_width, view_height;

   if (Vector->ParentView) {
      if (Vector->ParentView->vpDimensions & DMF_WIDTH) view_width = Vector->ParentView->vpFixedWidth;
      else if (Vector->ParentView->vpViewWidth > 0) view_width = Vector->ParentView->vpViewWidth;
      else view_width = Vector->Scene->PageWidth;

      if (Vector->ParentView->vpDimensions & DMF_HEIGHT) view_height = Vector->ParentView->vpFixedHeight;
      else if (Vector->ParentView->vpViewHeight > 0) view_height = Vector->ParentView->vpViewHeight;
      else view_height = Vector->Scene->PageHeight;
   }
   else if (Vector->Scene) {
      view_width  = Vector->Scene->PageWidth;
      view_height = Vector->Scene->PageHeight;
   }
   else return;

   if ((Vector->Points) AND (Vector->TotalPoints >= 2)) {
      DOUBLE top = DBL_MAX, bottom = DBL_MIN; // This is for caching the polygon boundary.
      DOUBLE left = DBL_MAX, right = DBL_MIN;

      DOUBLE x = Vector->Points[0].X;
      DOUBLE y = Vector->Points[0].Y;
      if (Vector->Points[0].XRelative) x *= view_width;
      if (Vector->Points[0].YRelative) y *= view_height;
      Vector->BasePath->move_to(x, y);

      LONG i;
      for (i=1; i < Vector->TotalPoints; i++) {
         x = Vector->Points[i].X;
         y = Vector->Points[i].Y;
         if (Vector->Points[i].XRelative) x *= view_width;
         if (Vector->Points[i].YRelative) y *= view_height;

         if (Vector->Points[i].X < left)   left   = x;
         if (Vector->Points[i].Y < top)    top    = y;
         if (Vector->Points[i].X > right)  right  = x;
         if (Vector->Points[i].Y > bottom) bottom = y;
         Vector->BasePath->line_to(x, y);
      }

      if ((Vector->TotalPoints > 2) AND (Vector->Closed)) Vector->BasePath->close_polygon();

      // Cache the polygon boundary values.
      Vector->X1 = left;
      Vector->Y1 = top;
      Vector->X2 = right;
      Vector->Y2 = bottom;
   }
   else FMSG("gen_polygon","Not enough points defined.");
}

//****************************************************************************
// Converts a string of paired coordinates into a VectorPoint array.

static ERROR read_points(objVectorPoly *Self, struct VectorPoint **Array, LONG *PointCount, CSTRING Value)
{
   // Count the number of values (note that a point consists of 2 values)

   LONG pos;
   LONG count = 0;
   for (pos=0; Value[pos];) {
      if ((Value[pos] >= '0') AND (Value[pos] <= '9')) {
         count++;
         // Consume all characters up to the next comma or whitespace.
         while (Value[pos]) { if ((Value[pos] IS ',') OR (Value[pos] <= 0x20)) break; pos++; }
      }
      else pos++;
   }

   if (count >= MAX_POINTS) return ERR_InvalidValue;

   if (count >= 2) {
      LONG points = count>>1; // A point consists of 2 values.
      if (PointCount) *PointCount = points;
      if (!AllocMemory(sizeof(struct VectorPoint) * count, MEM_DATA, Array, NULL)) {
         LONG point = 0;
         LONG index = 0;
         for (pos=0; (Value[pos]) AND (point < points);) {
            if (((Value[pos] >= '0') AND (Value[pos] <= '9')) OR (Value[pos] IS '-') OR (Value[pos] IS '+')) {
               if (!(index & 0x1)) {
                  Array[0][point].X = StrToFloat(Value + pos);
               }
               else {
                  Array[0][point].Y = StrToFloat(Value + pos);
                  point++;
               }
               index++;
               while (Value[pos]) { if ((Value[pos] IS ',') OR (Value[pos] <= 0x20)) break; pos++; }
            }
            else pos++;
         }

         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else {
      FMSG("@","List of points requires a minimum of 2 number pairs.");
      return PostError(ERR_InvalidValue);
   }
}

//****************************************************************************

static ERROR POLYGON_Free(objVectorPoly *Self, APTR Void)
{
   if (Self->Points) { FreeResource(Self->Points); Self->Points = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Move: Moves a polygon to a new position.
-END-
*****************************************************************************/

static ERROR POLYGON_Move(objVectorPoly *Self, struct acMove *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   LONG i;

   // Check if any of the polygon's points are relative, in which case we have to cancel the move.
   for (i=0; i < Self->TotalPoints; i++) {
      if ((Self->Points[i].XRelative) OR (Self->Points[i].YRelative)) return ERR_InvalidValue;
   }

   for (i=0; i < Self->TotalPoints; i++) {
      Self->Points[i].X += Args->XChange;
      Self->Points[i].Y += Args->YChange;
   }

   // Alter the boundary.
   Self->X1 += Args->XChange;
   Self->Y1 += Args->YChange;
   Self->X2 += Args->XChange;
   Self->Y2 += Args->YChange;
   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Moves a polygon to a new central position.

This action will permanently modify the coordinates of a polygon so that they offset by the provided coordinate values.

The operation will abort if any of the points in the polygon are discovered to be relative coordinates.
-END-
*****************************************************************************/

static ERROR POLYGON_MoveToPoint(objVectorPoly *Self, struct acMoveToPoint *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   LONG i;

   // Check if any of the polygon's points are relative, in which case we have to cancel the move.
   for (i=0; i < Self->TotalPoints; i++) {
      if ((Self->Points[i].XRelative) OR (Self->Points[i].YRelative)) return ERR_InvalidValue;
   }

   // The provided (X,Y) coordinates will be treated as the polygon's new central position.

   if (Args->Flags & MTF_X) {
      DOUBLE center_x = (Self->X2 - Self->X1) * 0.5;
      DOUBLE xchange = Args->X - center_x;
      for (i=0; i < Self->TotalPoints; i++) {
         Self->Points[i].X += xchange;
         Self->Points[i].XRelative = (Args->Flags & MTF_RELATIVE) ? TRUE : FALSE;
      }
      Self->X1 += xchange; // Alter the boundary.
      Self->X2 += xchange;
   }

   if (Args->Flags & MTF_Y) {
      DOUBLE center_y = (Self->Y2 - Self->Y1) * 0.5;
      DOUBLE ychange = Args->Y - center_y;
      for (i=0; i < Self->TotalPoints; i++) Self->Points[i].Y += ychange;
      Self->Y1 += ychange; // Alter the boundary.
      Self->Y2 += ychange;
   }

   mark_dirty(Self, RC_TRANSFORM);
   return ERR_Okay;
}

//****************************************************************************

static ERROR POLYGON_NewObject(objVectorPoly *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(struct rkVector *))&generate_polygon;
   Self->Closed = TRUE;
   Self->TotalPoints = 2;
   if (AllocMemory(sizeof(struct VectorPoint) * Self->TotalPoints, MEM_DATA, &Self->Points, NULL)) return ERR_AllocMemory;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Resize: Resize the polygon by its width and height.

This action will resize a polygon by adjusting all of its existing points.  The points are rescaled to guarantee that
they are within the provided dimensions.

If a Width and/or Height value of zero is passed, no scaling on the associated axis will occur.

*****************************************************************************/

static ERROR POLYGON_Resize(objVectorPoly *Self, struct acResize *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   DOUBLE current_width = Self->X2 - Self->X1;
   DOUBLE current_height = Self->Y2 - Self->Y1;
   DOUBLE xratio = (Args->Width > 0) ? (current_width / Args->Width) : current_width;
   DOUBLE yratio = (Args->Height > 0) ? (current_height / Args->Height) : current_height;

   LONG i;
   for (i=0; i < Self->TotalPoints; i++) {
      Self->Points[i].X = Self->Points[i].X * xratio;
      Self->Points[i].Y = Self->Points[i].Y * yratio;
   }

   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Closed: If TRUE, the polygon will be closed between the start and end points.

Set the Closed field to TRUE to ensure that the polygon is closed between the start and end points.  This behaviour is
the default.  If FALSE, the polygon will not be closed, which results in the equivalent of the SVG polyline type.

*****************************************************************************/

static ERROR POLY_GET_Closed(objVectorPoly *Self, LONG *Value)
{
   *Value = Self->Closed;
   return ERR_Okay;
}

static ERROR POLY_SET_Closed(objVectorPoly *Self, LONG Value)
{
   if (Value) Self->Closed = TRUE;
   else Self->Closed = FALSE;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
PointsArray: A series of numbered pairs that define the polygon.

The PointsArray field can be set with a &VectorPoint array that defines the shape of a polygon.  A minimum of two
points is required for the shape to be valid.  The &VectorPoint structure consists of the following fields:

&VectorPoint

*****************************************************************************/

static ERROR POLY_GET_PointsArray(objVectorPoly *Self, struct VectorPoint **Value, LONG *Elements)
{
   *Value = Self->Points;
   *Elements = Self->TotalPoints;
   return ERR_Okay;
}

static ERROR POLY_SET_PointsArray(objVectorPoly *Self, struct VectorPoint *Value, LONG Elements)
{
   if (Elements >= 2) {
      struct VectorPoint *points;
      if (!AllocMemory(sizeof(struct VectorPoint) * Elements, MEM_DATA|MEM_NO_CLEAR, &points, NULL)) {
         CopyMemory(Value, points, sizeof(struct VectorPoint) * Elements);
         Self->Points = points;
         Self->TotalPoints = Elements;
         reset_path(Self);
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
-FIELD-
Points: A series of (X,Y) coordinates that define the polygon.

The Points field can be set with a series of (X,Y) coordinates that will define the polygon's shape.  A minimum of two
numbered pairs will be required to define a valid polygon.  Each point must be separated with either white-space or
a comma.

*****************************************************************************/

static ERROR POLY_SET_Points(objVectorPoly *Self, CSTRING Value)
{
   ERROR error;
   struct VectorPoint *points;
   LONG total;
   if (!(error = read_points(Self, &points, &total, Value))) {
      if (Self->Points) FreeResource(Self->Points);
      Self->Points = points;
      Self->TotalPoints = total;
      reset_path(Self);
   }
   return error;
}

/*****************************************************************************
-FIELD-
TotalPoints: The total number of coordinates defined in the Points field.

TotalPoints is a read-only field value that reflects the total number of coordinates that have been set in the
#Points array.

*****************************************************************************/

static ERROR POLY_GET_TotalPoints(objVectorPoly *Self, LONG *Value)
{
   *Value = Self->TotalPoints;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
X1: Defines the X coordinate of the first point.

This field defines the X coordinate of the first point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.

*****************************************************************************/

static ERROR POLY_GET_X1(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val = Self->Points[0].X;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Points[0].XRelative)) val = val * 100;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_X1(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Points[0].XRelative = TRUE;
   }
   else Self->Points[0].XRelative = FALSE;
   Self->Points[0].X = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
X2: Defines the X coordinate of the second point.

This field defines the X coordinate of the second point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.

*****************************************************************************/

static ERROR POLY_GET_X2(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val = Self->Points[1].X;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Points[1].XRelative)) val = val * 100;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_X2(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Points[1].XRelative = TRUE;
   }
   else Self->Points[1].XRelative = FALSE;
   Self->Points[1].X = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Y1: Defines the Y coordinate of the first point.

This field defines the Y coordinate of the first point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.

*****************************************************************************/

static ERROR POLY_GET_Y1(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val = Self->Points[0].Y;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Points[0].YRelative)) val = val * 100;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_Y1(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Points[0].YRelative = TRUE;
   }
   else Self->Points[0].YRelative = FALSE;
   Self->Points[0].Y = val;
   reset_path(Self);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Y2: Defines the Y coordinate of the second point.

This field defines the Y coordinate of the second point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.
-END-
*****************************************************************************/

static ERROR POLY_GET_Y2(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val = Self->Points[1].Y;
   if ((Value->Type & FD_PERCENTAGE) AND (Self->Points[1].YRelative)) val = val * 100;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_Y2(objVectorPoly *Self, struct Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return PostError(ERR_FieldTypeMismatch);

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Points[1].YRelative = TRUE;
   }
   else Self->Points[1].YRelative = FALSE;
   Self->Points[1].Y = val;
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static const struct ActionArray clPolygonActions[] = {
   { AC_Free,        (APTR)POLYGON_Free },
   { AC_NewObject,   (APTR)POLYGON_NewObject },
   { AC_Move,        (APTR)POLYGON_Move },
   { AC_MoveToPoint, (APTR)POLYGON_MoveToPoint },
   //{ AC_Redimension, (APTR)POLYGON_Redimension },
   { AC_Resize,      (APTR)POLYGON_Resize },
   { 0, NULL }
};

static const struct FieldArray clPolygonFields[] = {
   { "Closed",      FDF_VIRTUAL|FDF_LONG|FD_RW,                 0, (APTR)POLY_GET_Closed, (APTR)POLY_SET_Closed },
   { "PointsArray", FDF_VIRTUAL|FDF_ARRAY|FDF_POINTER|FDF_RW,   0, (APTR)POLY_GET_PointsArray, (APTR)POLY_SET_PointsArray },
   { "Points",      FDF_VIRTUAL|FDF_STRING|FDF_W,               0, NULL, (APTR)POLY_SET_Points },
   { "TotalPoints", FDF_VIRTUAL|FDF_LONG|FDF_R,                 0, (APTR)POLY_GET_TotalPoints, NULL },
   { "X1",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, 0, (APTR)POLY_GET_X1, (APTR)POLY_SET_X1 },
   { "Y1",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, 0, (APTR)POLY_GET_Y1, (APTR)POLY_SET_Y1 },
   { "X2",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, 0, (APTR)POLY_GET_X2, (APTR)POLY_SET_X2 },
   { "Y2",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, 0, (APTR)POLY_GET_Y2, (APTR)POLY_SET_Y2 },
   END_FIELD
};

static ERROR init_polygon(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorPolygon,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORPOLYGON,
      FID_Name|TSTRING,      "VectorPolygon",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clPolygonActions,
      FID_Fields|TARRAY,     clPolygonFields,
      FID_Size|TLONG,        sizeof(objVectorPoly),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
