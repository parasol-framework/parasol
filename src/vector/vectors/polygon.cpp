/*********************************************************************************************************************

-CLASS-
VectorPolygon: Extends the Vector class with support for generating polygons.

The VectorPolygon class provides support for three different types of vector:

<list type="sorted">
<li>Closed-point polygons consisting of at least 3 points.</li>
<li>Open polygons consisting of at least 3 points (a 'polyline' in SVG).</li>
<li>Single lines consisting of two points only (a 'line' in SVG).</li>
</list>

To create a polyline, set the #Closed field to `false`.  

To create a line, set the #Closed field to `false` and set only two points (#X1,#Y1) and (#X2,#Y2)

-END-

TODO: Add a SetPoint(DOUBLE X, DOUBLE Y) method for modifying existing points.

*********************************************************************************************************************/

#define MAX_POINTS 1024 * 16 // Maximum of 16k points per polygon object.

static void generate_polygon(extVectorPoly *Vector, agg::path_storage &Path)
{
   auto view_width = get_parent_width(Vector);
   auto view_height = get_parent_height(Vector);

   if ((Vector->Points) and (Vector->TotalPoints >= 2)) {
      pf::POINT<double> p = { Vector->Points[0].X, Vector->Points[0].Y };
      if (Vector->Points[0].XScaled) p.x *= view_width;
      if (Vector->Points[0].YScaled) p.y *= view_height;
      Path.move_to(p.x, p.y);

      auto min = p; // Record min and max for the boundary.
      auto max = p;
      auto last = p;

      for (LONG i=1; i < Vector->TotalPoints; i++) {
         p.x = Vector->Points[i].X;
         p.y = Vector->Points[i].Y;
         if (Vector->Points[i].XScaled) p.x *= view_width;
         if (Vector->Points[i].YScaled) p.y *= view_height;

         if (p.x < min.x) min.x = p.x;
         if (p.y < min.y) min.y = p.y;
         if (p.x > max.x) max.x = p.x;
         if (p.y > max.y) max.y = p.y;

         // A quirk of AGG is that it won't draw a line if the start and end points are equal.  This might
         // seem reasonable, but it has side-effects such as stroke end-caps not being drawn at all.  For the
         // time being we can avoid this problem by making a slight adjustment so that the points don't match.

         if (p == last) p.x += 0.000001;

         Path.line_to(p.x, p.y);
         last = p;
      }

      if ((Vector->TotalPoints > 2) and (Vector->Closed)) Path.close_polygon();

      Vector->Bounds = { min.x, min.y, max.x, max.y };
   }
   else Vector->Bounds = { 0, 0, 0, 0 };
}

//********************************************************************************************************************
// Converts a string of paired coordinates into a VectorPoint array.

static ERR read_points(extVectorPoly *Self, VectorPoint **Array, LONG *PointCount, CSTRING Value)
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

   if (count >= MAX_POINTS) return ERR::InvalidValue;

   if (count >= 2) {
      LONG points = count>>1; // A point consists of 2 values.
      if (PointCount) *PointCount = points;
      if (AllocMemory(sizeof(VectorPoint) * count, MEM::DATA, Array) IS ERR::Okay) {
         LONG point = 0;
         LONG index = 0;
         for (LONG pos=0; (Value[pos]) and (point < points);) {
            if (((Value[pos] >= '0') and (Value[pos] <= '9')) or (Value[pos] IS '-') or (Value[pos] IS '+')) {
               if (!(index & 0x1)) {
                  Array[0][point].X = strtod(Value + pos, NULL);
               }
               else {
                  Array[0][point].Y = strtod(Value + pos, NULL);
                  point++;
               }
               index++;
               while (Value[pos]) { if ((Value[pos] IS ',') or (Value[pos] <= 0x20)) break; pos++; }
            }
            else pos++;
         }

         return ERR::Okay;
      }
      else return ERR::AllocMemory;
   }
   else {
      log.traceWarning("List of points requires a minimum of 2 number pairs.");
      return log.warning(ERR::InvalidValue);
   }
}

//********************************************************************************************************************

static ERR POLYGON_Free(extVectorPoly *Self)
{
   if (Self->Points) { FreeResource(Self->Points); Self->Points = NULL; }
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Move: Moves a polygon to a new position.
-END-
*********************************************************************************************************************/

static ERR POLYGON_Move(extVectorPoly *Self, struct acMove *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   // If any of the polygon's points are relative then we have to cancel the move.
   for (LONG i=0; i < Self->TotalPoints; i++) {
      if ((Self->Points[i].XScaled) or (Self->Points[i].YScaled)) return ERR::InvalidValue;
   }

   for (LONG i=0; i < Self->TotalPoints; i++) {
      Self->Points[i].X += Args->DeltaX;
      Self->Points[i].Y += Args->DeltaY;
   }

   Self->Bounds.left   += Args->DeltaX;
   Self->Bounds.top    += Args->DeltaY;
   Self->Bounds.right  += Args->DeltaX;
   Self->Bounds.bottom += Args->DeltaY;

   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Moves a polygon to a new central position.

This action will permanently modify the coordinates of a polygon so that they offset by the provided coordinate values.

The operation will abort if any of the points in the polygon are discovered to be relative coordinates.
-END-
*********************************************************************************************************************/

static ERR POLYGON_MoveToPoint(extVectorPoly *Self, struct acMoveToPoint *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   LONG i;

   // Check if any of the polygon's points are relative, in which case we have to cancel the move.
   for (i=0; i < Self->TotalPoints; i++) {
      if ((Self->Points[i].XScaled) or (Self->Points[i].YScaled)) return ERR::InvalidValue;
   }

   // The provided (X,Y) coordinates will be treated as the polygon's new central position.

   if ((Args->Flags & MTF::X) != MTF::NIL) {
      DOUBLE center_x = Self->Bounds.width() * 0.5;
      DOUBLE xchange = Args->X - center_x;
      for (i=0; i < Self->TotalPoints; i++) {
         Self->Points[i].X += xchange;
         Self->Points[i].XScaled = ((Args->Flags & MTF::RELATIVE) != MTF::NIL);
      }
      Self->Bounds.left += xchange;
      Self->Bounds.right += xchange;
   }

   if ((Args->Flags & MTF::Y) != MTF::NIL) {
      DOUBLE center_y = Self->Bounds.height() * 0.5;
      DOUBLE ychange = Args->Y - center_y;
      for (i=0; i < Self->TotalPoints; i++) Self->Points[i].Y += ychange;
      Self->Bounds.top += ychange;
      Self->Bounds.bottom += ychange;
   }

   reset_path(Self);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR POLYGON_NewObject(extVectorPoly *Self)
{
   Self->GeneratePath = (void (*)(extVector *, agg::path_storage &))&generate_polygon;
   Self->Closed       = TRUE;
   Self->TotalPoints  = 2;
   if (AllocMemory(sizeof(VectorPoint) * Self->TotalPoints, MEM::DATA, &Self->Points) != ERR::Okay) return ERR::AllocMemory;
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Resize the polygon by its width and height.

This action will resize a polygon by adjusting all of its existing points.  The points are rescaled to guarantee that
they are within the provided dimensions.

If a Width and/or Height value of zero is passed, no scaling on the associated axis will occur.

*********************************************************************************************************************/

static ERR POLYGON_Resize(extVectorPoly *Self, struct acResize *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   DOUBLE current_width = Self->Bounds.width();
   DOUBLE current_height = Self->Bounds.height();
   DOUBLE xratio = (Args->Width > 0) ? (current_width / Args->Width) : current_width;
   DOUBLE yratio = (Args->Height > 0) ? (current_height / Args->Height) : current_height;

   for (LONG i=0; i < Self->TotalPoints; i++) {
      Self->Points[i].X = Self->Points[i].X * xratio;
      Self->Points[i].Y = Self->Points[i].Y * yratio;
   }

   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Closed: If `true`, the polygon will be closed between the start and end points.

Set the Closed field to `true` to ensure that the polygon is closed between the start and end points.  This behaviour is
the default.  If `false`, the polygon will not be closed, which results in the equivalent of the SVG polyline type.

*********************************************************************************************************************/

static ERR POLY_GET_Closed(extVectorPoly *Self, LONG *Value)
{
   *Value = Self->Closed;
   return ERR::Okay;
}

static ERR POLY_SET_Closed(extVectorPoly *Self, LONG Value)
{
   if (Value) Self->Closed = true;
   else Self->Closed = false;
   reset_path(Self);
   return ERR::Okay;
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

static ERR POLY_GET_PathLength(extVectorPoly *Self, LONG *Value)
{
   *Value = Self->PathLength;
   return ERR::Okay;
}

static ERR POLY_SET_PathLength(extVectorPoly *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PathLength = Value;
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
PointsArray: A series of numbered pairs that define the polygon.

The PointsArray field can be set with a !VectorPoint array that defines the shape of a polygon.  A minimum of two
points is required for the shape to be valid.  The !VectorPoint structure consists of the following fields:

!VectorPoint

*********************************************************************************************************************/

static ERR POLY_GET_PointsArray(extVectorPoly *Self, VectorPoint **Value, LONG *Elements)
{
   *Value = Self->Points;
   *Elements = Self->TotalPoints;
   return ERR::Okay;
}

static ERR POLY_SET_PointsArray(extVectorPoly *Self, VectorPoint *Value, LONG Elements)
{
   if (Elements >= 2) {
      VectorPoint *points;
      if (AllocMemory(sizeof(VectorPoint) * Elements, MEM::DATA|MEM::NO_CLEAR, &points) IS ERR::Okay) {
         CopyMemory(Value, points, sizeof(VectorPoint) * Elements);
         Self->Points = points;
         Self->TotalPoints = Elements;
         reset_path(Self);
         return ERR::Okay;
      }
      else return ERR::AllocMemory;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Points: A series of (X,Y) coordinates that define the polygon.

The Points field can be set with a series of `(X, Y)` coordinates that will define the polygon's shape.  A minimum of 
two numbered pairs will be required to define a valid polygon.  Each point must be separated with either white-space or
a comma.

*********************************************************************************************************************/

static ERR POLY_SET_Points(extVectorPoly *Self, CSTRING Value)
{
   ERR error;
   VectorPoint *points;
   LONG total;
   if ((error = read_points(Self, &points, &total, Value)) IS ERR::Okay) {
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

static ERR POLY_GET_TotalPoints(extVectorPoly *Self, LONG *Value)
{
   *Value = Self->TotalPoints;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
X1: Defines the X coordinate of the first point.

This field defines the X coordinate of the first point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Scaled values are supported if the value is a defined as
a percentage.

*********************************************************************************************************************/

static ERR POLY_GET_X1(extVectorPoly *Self, Unit *Value)
{
   Value->set(Self->Points[0].X);
   return ERR::Okay;
}

static ERR POLY_SET_X1(extVectorPoly *Self, Unit &Value)
{
   Self->Points[0].XScaled = Value.scaled();
   Self->Points[0].X = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
X2: Defines the X coordinate of the second point.

This field defines the X coordinate of the second point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Scaled values are supported if the value is a defined as
a percentage.

*********************************************************************************************************************/

static ERR POLY_GET_X2(extVectorPoly *Self, Unit *Value)
{
   Value->set(Self->Points[1].X);
   return ERR::Okay;
}

static ERR POLY_SET_X2(extVectorPoly *Self, Unit &Value)
{
   Self->Points[1].XScaled = Value.scaled();
   Self->Points[1].X = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Y1: Defines the Y coordinate of the first point.

This field defines the Y coordinate of the first point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Scaled values are supported if the value is a defined as
a percentage.

*********************************************************************************************************************/

static ERR POLY_GET_Y1(extVectorPoly *Self, Unit *Value)
{
   Value->set(Self->Points[0].Y);
   return ERR::Okay;
}

static ERR POLY_SET_Y1(extVectorPoly *Self, Unit &Value)
{
   Self->Points[0].YScaled = Value.scaled();
   Self->Points[0].Y = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Y2: Defines the Y coordinate of the second point.

This field defines the Y coordinate of the second point of the polygon.  It is recommended that this field is only used
when creating a VectorPolygon that will be used to draw a single line.

By default the value will be treated as a fixed coordinate.  Scaled values are supported if the value is a defined as
a percentage.
-END-
*********************************************************************************************************************/

static ERR POLY_GET_Y2(extVectorPoly *Self, Unit *Value)
{
   Value->set(Self->Points[1].Y);
   return ERR::Okay;
}

static ERR POLY_SET_Y2(extVectorPoly *Self, Unit &Value)
{
   Self->Points[1].YScaled = Value.scaled();
   Self->Points[1].Y = Value;
   reset_path(Self);
   return ERR::Okay;
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
   { "X1",          FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, POLY_GET_X1, POLY_SET_X1 },
   { "Y1",          FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, POLY_GET_Y1, POLY_SET_Y1 },
   { "X2",          FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, POLY_GET_X2, POLY_SET_X2 },
   { "Y2",          FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, POLY_GET_Y2, POLY_SET_Y2 },
   END_FIELD
};

//********************************************************************************************************************

static ERR init_polygon(void)
{
   clVectorPolygon = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::VECTOR),
      fl::ClassID(CLASSID::VECTORPOLYGON),
      fl::Name("VectorPolygon"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clPolygonActions),
      fl::Fields(clPolygonFields),
      fl::Size(sizeof(extVectorPoly)),
      fl::Path(MOD_PATH));

   return clVectorPolygon ? ERR::Okay : ERR::AddClass;
}
