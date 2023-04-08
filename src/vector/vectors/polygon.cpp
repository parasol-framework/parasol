/*********************************************************************************************************************

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

*********************************************************************************************************************/

#define MAX_POINTS 1024 * 16 // Maximum of 16k points per polygon object.

static void generate_polygon(extVectorPoly *Vector)
{
   DOUBLE view_width, view_height;
   get_parent_size(Vector, view_width, view_height);

   if ((Vector->Points) and (Vector->TotalPoints >= 2)) {
      DOUBLE x = Vector->Points[0].X;
      DOUBLE y = Vector->Points[0].Y;
      if (Vector->Points[0].XRelative) x *= view_width;
      if (Vector->Points[0].YRelative) y *= view_height;
      Vector->BasePath.move_to(x, y);

      DOUBLE min_x = x, max_x = x, min_y = y, max_y = y;

      for (LONG i=1; i < Vector->TotalPoints; i++) {
         x = Vector->Points[i].X;
         y = Vector->Points[i].Y;
         if (Vector->Points[i].XRelative) x *= view_width;
         if (Vector->Points[i].YRelative) y *= view_height;

         if (x < min_x) min_x = x;
         if (y < min_y) min_y = y;
         if (x > max_x) max_x = x;
         if (y > max_y) max_y = y;
         Vector->BasePath.line_to(x, y);
      }

      if ((Vector->TotalPoints > 2) and (Vector->Closed)) Vector->BasePath.close_polygon();

      Vector->BX1 = min_x;
      Vector->BY1 = min_y;
      Vector->BX2 = max_x;
      Vector->BY2 = max_y;
   }
   else {
      Vector->BX1 = 0;
      Vector->BY1 = 0;
      Vector->BX2 = 0;
      Vector->BY2 = 0;
   }
}

//********************************************************************************************************************
// Converts a string of paired coordinates into a VectorPoint array.

static ERROR read_points(extVectorPoly *Self, VectorPoint **Array, LONG *PointCount, CSTRING Value)
{
   pf::Log log(__FUNCTION__);

   // Count the number of values (note that a point consists of 2 values)

   LONG count = 0;
   for (LONG pos=0; Value[pos];) {
      if ((Value[pos] >= '0') and (Value[pos] <= '9')) {
         count++;
         // Consume all characters up to the next comma or whitespace.
         while (Value[pos]) { if ((Value[pos] IS ',') or (Value[pos] <= 0x20)) break; pos++; }
      }
      else pos++;
   }

   if (count >= MAX_POINTS) return ERR_InvalidValue;

   if (count >= 2) {
      LONG points = count>>1; // A point consists of 2 values.
      if (PointCount) *PointCount = points;
      if (!AllocMemory(sizeof(VectorPoint) * count, MEM_DATA, Array)) {
         LONG point = 0;
         LONG index = 0;
         for (LONG pos=0; (Value[pos]) and (point < points);) {
            if (((Value[pos] >= '0') and (Value[pos] <= '9')) or (Value[pos] IS '-') or (Value[pos] IS '+')) {
               if (!(index & 0x1)) {
                  Array[0][point].X = StrToFloat(Value + pos);
               }
               else {
                  Array[0][point].Y = StrToFloat(Value + pos);
                  point++;
               }
               index++;
               while (Value[pos]) { if ((Value[pos] IS ',') or (Value[pos] <= 0x20)) break; pos++; }
            }
            else pos++;
         }

         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else {
      log.traceWarning("List of points requires a minimum of 2 number pairs.");
      return log.warning(ERR_InvalidValue);
   }
}

//********************************************************************************************************************

static ERROR POLYGON_Free(extVectorPoly *Self, APTR Void)
{
   if (Self->Points) { FreeResource(Self->Points); Self->Points = NULL; }
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Move: Moves a polygon to a new position.
-END-
*********************************************************************************************************************/

static ERROR POLYGON_Move(extVectorPoly *Self, struct acMove *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   // If any of the polygon's points are relative then we have to cancel the move.
   for (LONG i=0; i < Self->TotalPoints; i++) {
      if ((Self->Points[i].XRelative) or (Self->Points[i].YRelative)) return ERR_InvalidValue;
   }

   for (LONG i=0; i < Self->TotalPoints; i++) {
      Self->Points[i].X += Args->DeltaX;
      Self->Points[i].Y += Args->DeltaY;
   }

   Self->BX1 += Args->DeltaX;
   Self->BY1 += Args->DeltaY;
   Self->BX2 += Args->DeltaX;
   Self->BY2 += Args->DeltaY;

   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Moves a polygon to a new central position.

This action will permanently modify the coordinates of a polygon so that they offset by the provided coordinate values.

The operation will abort if any of the points in the polygon are discovered to be relative coordinates.
-END-
*********************************************************************************************************************/

static ERROR POLYGON_MoveToPoint(extVectorPoly *Self, struct acMoveToPoint *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   LONG i;

   // Check if any of the polygon's points are relative, in which case we have to cancel the move.
   for (i=0; i < Self->TotalPoints; i++) {
      if ((Self->Points[i].XRelative) or (Self->Points[i].YRelative)) return ERR_InvalidValue;
   }

   // The provided (X,Y) coordinates will be treated as the polygon's new central position.

   if (Args->Flags & MTF_X) {
      DOUBLE center_x = (Self->BX2 - Self->BX1) * 0.5;
      DOUBLE xchange = Args->X - center_x;
      for (i=0; i < Self->TotalPoints; i++) {
         Self->Points[i].X += xchange;
         Self->Points[i].XRelative = (Args->Flags & MTF_RELATIVE) ? TRUE : FALSE;
      }
      Self->BX1 += xchange;
      Self->BX2 += xchange;
   }

   if (Args->Flags & MTF_Y) {
      DOUBLE center_y = (Self->BY2 - Self->BY1) * 0.5;
      DOUBLE ychange = Args->Y - center_y;
      for (i=0; i < Self->TotalPoints; i++) Self->Points[i].Y += ychange;
      Self->BY1 += ychange;
      Self->BY2 += ychange;
   }

   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR POLYGON_NewObject(extVectorPoly *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(extVector *))&generate_polygon;
   Self->Closed       = TRUE;
   Self->TotalPoints  = 2;
   if (AllocMemory(sizeof(VectorPoint) * Self->TotalPoints, MEM_DATA, &Self->Points)) return ERR_AllocMemory;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Resize the polygon by its width and height.

This action will resize a polygon by adjusting all of its existing points.  The points are rescaled to guarantee that
they are within the provided dimensions.

If a Width and/or Height value of zero is passed, no scaling on the associated axis will occur.

*********************************************************************************************************************/

static ERROR POLYGON_Resize(extVectorPoly *Self, struct acResize *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   DOUBLE current_width = Self->BX2 - Self->BX1;
   DOUBLE current_height = Self->BY2 - Self->BY1;
   DOUBLE xratio = (Args->Width > 0) ? (current_width / Args->Width) : current_width;
   DOUBLE yratio = (Args->Height > 0) ? (current_height / Args->Height) : current_height;

   for (LONG i=0; i < Self->TotalPoints; i++) {
      Self->Points[i].X = Self->Points[i].X * xratio;
      Self->Points[i].Y = Self->Points[i].Y * yratio;
   }

   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Closed: If TRUE, the polygon will be closed between the start and end points.

Set the Closed field to TRUE to ensure that the polygon is closed between the start and end points.  This behaviour is
the default.  If FALSE, the polygon will not be closed, which results in the equivalent of the SVG polyline type.

*********************************************************************************************************************/

static ERROR POLY_GET_Closed(extVectorPoly *Self, LONG *Value)
{
   *Value = Self->Closed;
   return ERR_Okay;
}

static ERROR POLY_SET_Closed(extVectorPoly *Self, LONG Value)
{
   if (Value) Self->Closed = TRUE;
   else Self->Closed = FALSE;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
PathLength: Calibrates the user agent's distance-along-a-path calculations with that of the author.

The author's computation of the total length of the path, in user units. This value is used to calibrate the user
agent's own distance-along-a-path calculations with that of the author. The user agent will scale all
distance-along-a-path computations by the ratio of PathLength to the user agent's own computed value for total path
length.  This feature potentially affects calculations for text on a path, motion animation and various stroke
operations.

*********************************************************************************************************************/

static ERROR POLY_GET_PathLength(extVectorPoly *Self, LONG *Value)
{
   *Value = Self->PathLength;
   return ERR_Okay;
}

static ERROR POLY_SET_PathLength(extVectorPoly *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PathLength = Value;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
PointsArray: A series of numbered pairs that define the polygon.

The PointsArray field can be set with a &VectorPoint array that defines the shape of a polygon.  A minimum of two
points is required for the shape to be valid.  The &VectorPoint structure consists of the following fields:

&VectorPoint

*********************************************************************************************************************/

static ERROR POLY_GET_PointsArray(extVectorPoly *Self, VectorPoint **Value, LONG *Elements)
{
   *Value = Self->Points;
   *Elements = Self->TotalPoints;
   return ERR_Okay;
}

static ERROR POLY_SET_PointsArray(extVectorPoly *Self, VectorPoint *Value, LONG Elements)
{
   if (Elements >= 2) {
      VectorPoint *points;
      if (!AllocMemory(sizeof(VectorPoint) * Elements, MEM_DATA|MEM_NO_CLEAR, &points)) {
         CopyMemory(Value, points, sizeof(VectorPoint) * Elements);
         Self->Points = points;
         Self->TotalPoints = Elements;
         reset_path(Self);
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Points: A series of (X,Y) coordinates that define the polygon.

The Points field can be set with a series of (X,Y) coordinates that will define the polygon's shape.  A minimum of two
numbered pairs will be required to define a valid polygon.  Each point must be separated with either white-space or
a comma.

*********************************************************************************************************************/

static ERROR POLY_SET_Points(extVectorPoly *Self, CSTRING Value)
{
   ERROR error;
   VectorPoint *points;
   LONG total;
   if (!(error = read_points(Self, &points, &total, Value))) {
      if (Self->Points) FreeResource(Self->Points);
      Self->Points = points;
      Self->TotalPoints = total;
      reset_path(Self);
   }
   return error;
}

/*********************************************************************************************************************
-FIELD-
TotalPoints: The total number of coordinates defined in the Points field.

TotalPoints is a read-only field value that reflects the total number of coordinates that have been set in the
#Points array.  The minimum value is 2.

*********************************************************************************************************************/

static ERROR POLY_GET_TotalPoints(extVectorPoly *Self, LONG *Value)
{
   *Value = Self->TotalPoints;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
X1: Defines the X coordinate of the first point.

This field defines the X coordinate of the first point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.

*********************************************************************************************************************/

static ERROR POLY_GET_X1(extVectorPoly *Self, Variable *Value)
{
   DOUBLE val = Self->Points[0].X;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_X1(extVectorPoly *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) Self->Points[0].XRelative = TRUE;
   else Self->Points[0].XRelative = FALSE;
   Self->Points[0].X = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
X2: Defines the X coordinate of the second point.

This field defines the X coordinate of the second point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.

*********************************************************************************************************************/

static ERROR POLY_GET_X2(extVectorPoly *Self, Variable *Value)
{
   DOUBLE val = Self->Points[1].X;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_X2(extVectorPoly *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) Self->Points[1].XRelative = TRUE;
   else Self->Points[1].XRelative = FALSE;
   Self->Points[1].X = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Y1: Defines the Y coordinate of the first point.

This field defines the Y coordinate of the first point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.

*********************************************************************************************************************/

static ERROR POLY_GET_Y1(extVectorPoly *Self, Variable *Value)
{
   DOUBLE val = Self->Points[0].Y;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_Y1(extVectorPoly *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) Self->Points[0].YRelative = TRUE;
   else Self->Points[0].YRelative = FALSE;
   Self->Points[0].Y = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Y2: Defines the Y coordinate of the second point.

This field defines the Y coordinate of the second point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Relative values are supported if the value is a defined as
a percentage.
-END-
*********************************************************************************************************************/

static ERROR POLY_GET_Y2(extVectorPoly *Self, Variable *Value)
{
   DOUBLE val = Self->Points[1].Y;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR POLY_SET_Y2(extVectorPoly *Self, Variable *Value)
{
   pf::Log log;
   DOUBLE val;

   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else if (Value->Type & FD_STRING) val = strtod((CSTRING)Value->Pointer, NULL);
   else return log.warning(ERR_SetValueNotNumeric);

   if (Value->Type & FD_PERCENTAGE) Self->Points[1].YRelative = TRUE;
   else Self->Points[1].YRelative = FALSE;
   Self->Points[1].Y = val;
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static const ActionArray clPolygonActions[] = {
   { AC_Free,        POLYGON_Free },
   { AC_NewObject,   POLYGON_NewObject },
   { AC_Move,        POLYGON_Move },
   { AC_MoveToPoint, POLYGON_MoveToPoint },
   //{ AC_Redimension, POLYGON_Redimension },
   { AC_Resize,      POLYGON_Resize },
   { 0, NULL }
};

static const FieldArray clPolygonFields[] = {
   { "Closed",      FDF_VIRTUAL|FDF_LONG|FD_RW,                 POLY_GET_Closed, POLY_SET_Closed },
   { "PathLength",  FDF_VIRTUAL|FDF_LONG|FDF_RW,                POLY_GET_PathLength, POLY_SET_PathLength },
   { "PointsArray", FDF_VIRTUAL|FDF_ARRAY|FDF_POINTER|FDF_RW,   POLY_GET_PointsArray, POLY_SET_PointsArray },
   { "Points",      FDF_VIRTUAL|FDF_STRING|FDF_W,               NULL, POLY_SET_Points },
   { "TotalPoints", FDF_VIRTUAL|FDF_LONG|FDF_R,                 POLY_GET_TotalPoints },
   { "X1",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, POLY_GET_X1, POLY_SET_X1 },
   { "Y1",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, POLY_GET_Y1, POLY_SET_Y1 },
   { "X2",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, POLY_GET_X2, POLY_SET_X2 },
   { "Y2",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_RW, POLY_GET_Y2, POLY_SET_Y2 },
   END_FIELD
};

//********************************************************************************************************************

static ERROR init_polygon(void)
{
   clVectorPolygon = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::ClassID(ID_VECTORPOLYGON),
      fl::Name("VectorPolygon"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clPolygonActions),
      fl::Fields(clPolygonFields),
      fl::Size(sizeof(extVectorPoly)),
      fl::Path(MOD_PATH));

   return clVectorPolygon ? ERR_Okay : ERR_AddClass;
}
