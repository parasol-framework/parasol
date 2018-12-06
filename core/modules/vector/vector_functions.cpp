/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-MODULE-
Vector: Create, manipulate and draw vector graphics to bitmaps.

The Vector module exports a small number of functions to assist the @Vector class, as well as some primitive
functions for creating paths and rendering them to bitmaps.

-END-

*****************************************************************************/

class SimpleVector {
public:
   agg::path_storage mPath;
   agg::renderer_base<agg::pixfmt_rkl> mRenderer;
   agg::rasterizer_scanline_aa<> mRaster; // For rendering the scene.  Stores a copy of the path, and other values.

   SimpleVector() { }

   // Refer to scene_draw.cpp for DrawPath()
   void DrawPath(objBitmap *, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle, OBJECTPTR FillStyle);
};

// Resource management for the SimpleVector follows.  NB: This is a beta feature in the Core.

static void simplevector_free(APTR Address) {

}

static struct ResourceManager glResourceSimpleVector = {
   "SimpleVector",
   &simplevector_free
};

void set_memory_manager(APTR Address, struct ResourceManager *Manager)
{
   struct ResourceManager **address_mgr = (struct ResourceManager **)((char *)Address - sizeof(LONG) - sizeof(LONG) - sizeof(struct ResourceManager *));
   address_mgr[0] = Manager;
}

static SimpleVector * new_simplevector(void)
{
   SimpleVector *vector;
   if (AllocMemory(sizeof(SimpleVector), MEM_DATA|MEM_MANAGED, &vector, NULL) != ERR_Okay) return NULL;
   set_memory_manager(vector, &glResourceSimpleVector);
   new(vector) SimpleVector;
   return vector;
}

//****************************************************************************

#include "module_def.c"

//****************************************************************************

ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, glFunctions);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
ApplyPath: Copy a pre-generated or custom path to a VectorPath object.

Any path originating from ~GeneratePath(), ~GenerateEllipse() or ~GenerateRectangle() can be applied to a VectorPath
object by calling ApplyPath().  The source Path can then be deallocated with ~FreePath() if it is no longer required.

This method is particularly useful when paths need to be generated or changed in real-time and the alternative of
processing the path as a string is detrimental to performance.

-INPUT-
ptr Path: The source path to be copied.
obj VectorPath: The target VectorPath object.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR vecApplyPath(class SimpleVector *Vector, objVectorPath *VectorPath)
{
   if ((!Vector) or (!VectorPath)) return ERR_NullArgs;
   if (VectorPath->Head.SubID != ID_VECTORPATH) return ERR_Args;

   SetField(VectorPath, FID_Sequence, NULL); // Clear any pre-existing path information.

   if (VectorPath->CustomPath) { delete VectorPath->CustomPath; VectorPath->CustomPath = NULL; }
   VectorPath->CustomPath = new (std::nothrow) agg::path_storage(Vector->mPath);

   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
ArcTo: Alter a path by setting an arc-to command at the current vertex position.

This function will set an arc-to command at the current vertex.  It then increments the vertex position for the next
path command.

-INPUT-
ptr Path: The vector path to modify.
double RX: The horizontal radius of the arc.
double RY: The vertical radius of the arc.
double Angle: The angle of the arc, expressed in radians.
double X: The horizontal end point for the arc command.
double Y: The vertical end point for the arc command.
int(ARC) Flags: Optional flags.

*****************************************************************************/

static void vecArcTo(SimpleVector *Vector, DOUBLE RX, DOUBLE RY, DOUBLE Angle, DOUBLE X, DOUBLE Y, LONG Flags)
{
   Vector->mPath.arc_to(RX, RY, Angle, (Flags & ARC_LARGE) ? 1 : 0, (Flags & ARC_SWEEP) ? 1 : 0, X, Y);
}

/*****************************************************************************

-FUNCTION-
ClosePath: Close the path by connecting the beginning and end points.

This function will set a close-path command at the current vertex.  It then increments the vertex position
for the next path command.

Note that closing a path does not necessarily terminate the vector.  Further paths can be added to the sequence and
interesting effects can be created by taking advantage of fill rules.

-INPUT-
ptr Path:  The vector path to modify.

*****************************************************************************/

static void vecClosePath(SimpleVector *Vector)
{
   Vector->mPath.close_polygon();
}

/*****************************************************************************

-FUNCTION-
Curve3: Alter a path by setting a quadratic bezier curve command at the current vertex position.

This function will set a quadratic bezier curve command at the current vertex.  It then increments the vertex position
for the next path command.

-INPUT-
ptr Path: The vector path to modify.
double CtrlX: Control point horizontal coordinate.
double CtrlY: Control point vertical coordinate.
double X: The horizontal end point for the curve3 command.
double Y: The vertical end point for the curve3 command.

*****************************************************************************/

static void vecCurve3(SimpleVector *Vector, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y)
{
   Vector->mPath.curve3(CtrlX, CtrlY, X, Y);
}

/*****************************************************************************

-FUNCTION-
Curve4: Alter a path by setting a curve4 command at the current vertex position.

This function will set a cubic bezier curve command at the current vertex.  It then increments the vertex position
for the next path command.

-INPUT-
ptr Path: The vector path to modify.
double CtrlX1: Control point 1 horizontal coordinate.
double CtrlY1: Control point 1 vertical coordinate.
double CtrlX2: Control point 2 horizontal coordinate.
double CtrlY2: Control point 2 vertical coordinate.
double X: The horizontal end point for the curve4 command.
double Y: The vertical end point for the curve4 command.

*****************************************************************************/

static void vecCurve4(SimpleVector *Vector, DOUBLE CtrlX1, DOUBLE CtrlY1, DOUBLE CtrlX2, DOUBLE CtrlY2, DOUBLE X, DOUBLE Y)
{
   Vector->mPath.curve4(CtrlX1, CtrlY1, CtrlX2, CtrlY2, X, Y);
}

/*****************************************************************************

-FUNCTION-
DrawPath: Draws a vector path to a target bitmap.

Use DrawPath() to draw a generated path to a Bitmap, using customised fill and stroke definitions.  This
functionality provides an effective alternative to configuring vector scenes for situations where only
simple vector shapes are required.  However, it is limited in that advanced rendering options and effects are not
available to the client.

A StrokeStyle and/or FillStyle will be required to render the path.  Valid styles are allocated and configured using
recognised vector style objects, specifically from the classes @VectorImage, @VectorPattern and @VectorGradient.  If a
fill or stroke operation is not required, set the relevant parameter to NULL.

-INPUT-
obj(Bitmap) Bitmap: Pointer to a target Bitmap object.
ptr Path: The vector path to render.
double StrokeWidth: The width of the stroke.  Set to 0 if no stroke is required.
obj StrokeStyle: Pointer to a valid object for stroke definition, or NULL if none required.
obj FillStyle: Pointer to a valid object for fill definition, or NULL if none required.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR vecDrawPath(objBitmap *Bitmap, class SimpleVector *Path, DOUBLE StrokeWidth, OBJECTPTR StrokeStyle,
   OBJECTPTR FillStyle)
{
   if ((!Bitmap) or (!Path)) return LogError(ERH_Function, ERR_NullArgs);
   if (StrokeWidth < 0.001) StrokeStyle = NULL;

   if ((!StrokeStyle) and (!FillStyle)) {
      FMSG("@DrawPath()","No Stroke or Fill parameter provided.");
      return ERR_Okay;
   }

   Path->DrawPath(Bitmap, StrokeWidth, StrokeStyle, FillStyle);
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
FreePath: Remove a generated path.

Deallocates paths generated by the Vector module, such as ~GeneratePath().

-INPUT-
ptr Path: Pointer to the path to deallocate.

*****************************************************************************/

static void vecFreePath(APTR Path)
{
   if (!Path) return;
   // NB: Refer to the deallocator for SimpleVector for anything relating to additional resource deallocation.
   FreeResource(Path);
}

/*****************************************************************************

-FUNCTION-
GetVertex: Retrieve the coordinates of the current vertex.

The coordinates of the current vertex are returned by this function in the X and Y parameters.  In addition, the
internal command number for that vertex is the return value.

-INPUT-
ptr Path: The vector path to modify.
&double X: Pointer to a DOUBLE that will receive the X coordinate value.
&double Y: Pointer to a DOUBLE that will receive the Y coordinate value.

-RESULT-
int: The internal command value for the vertex will be returned.

*****************************************************************************/

static LONG vecGetVertex(SimpleVector *Vector, DOUBLE *X, DOUBLE *Y)
{
   return Vector->mPath.vertex(X, Y);
}

/*****************************************************************************

-FUNCTION-
GenerateEllipse: Generates an elliptical path.

Use GenerateEllipse() to create an elliptical path suitable for passing to vector functions that receive a Path
parameter.  The path must be manually deallocated with ~FreePath() once it is no longer required.

-INPUT-
double CX: Horizontal center point of the ellipse.
double CY: Vertical center point of the ellipse.
double RX: Horizontal radius of the ellipse.
double RY: Vertical radius of the ellipse.
int Vertices: Optional.  If >= 3, the total number of generated vertices will be limited to the specified value.
&ptr Path: A pointer variable that will receive the resulting path.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

static ERROR vecGenerateEllipse(DOUBLE CX, DOUBLE CY, DOUBLE RX, DOUBLE RY, LONG Vertices, APTR *Path)
{
   if (!Path) return ERR_NullArgs;

   SimpleVector *vector = new_simplevector();
   if (!vector) return ERR_AllocMemory;

#if 0
   // Bezier curves can produce a reasonable approximation of an ellipse, but in practice there is
   // both a noticeable loss of speed and path accuracy vs the point plotting method.

   const DOUBLE kappa = 0.5522848; // 4 * ((√(2) - 1) / 3)

   const DOUBLE ox = RX * kappa;  // control point offset horizontal
   const DOUBLE oy = RY * kappa;  // control point offset vertical
   const DOUBLE xe = CX + RX;
   const DOUBLE ye = CY + RY;

   vector->mPath.move_to(CX - RX, CY);
   vector->mPath.curve4(CX - RX, CY - oy, CX - ox, CY - RY, CX, CY - RY);
   vector->mPath.curve4(CX + ox, CY - RY, xe, CY - oy, xe, CY);
   vector->mPath.curve4(xe, CY + oy, CX + ox, ye, CX, ye);
   vector->mPath.curve4(CX - ox, ye, CX - RX, CY + oy, CX - RX, CY);
   vector->mPath.close_polygon();
#else
   ULONG steps;

   if (Vertices >= 3) steps = Vertices;
   else {
      const DOUBLE ra = (fabs(RX) + fabs(RY)) / 2.0;
      const DOUBLE da = acos(ra / (ra + 0.125)) * 2.0;
      steps = agg::uround(2.0 * agg::pi / da);
      if (steps < 3) steps = 3; // Because you need at least 3 vertices to create a shape.
   }

   for (ULONG step=0; step < steps; step++) {
      const DOUBLE angle = DOUBLE(step) / DOUBLE(steps) * 2.0 * agg::pi;
      const DOUBLE x = CX + cos(angle) * RX;
      const DOUBLE y = CY + sin(angle) * RY;
      if (step == 0) vector->mPath.move_to(x, y);
      else vector->mPath.line_to(x, y);
   }
   vector->mPath.close_polygon();
#endif

   *Path = vector;
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
GenerateRectangle: Generate a rectangular path at (x,y) with size (width,height).

Use GenerateRectangle() to create a rectangular path suitable for passing to vector functions that receive a Path
parameter.  The path must be manually deallocated with ~FreePath() once it is no longer required.

-INPUT-
double X: The horizontal position of the rectangle.
double Y: The vertical position of the rectangle.
double Width: The width of the rectangle.
double Height: The height of the rectangle.
&ptr Path: A pointer variable that will receive the resulting path.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

static ERROR vecGenerateRectangle(DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height, APTR *Path)
{
   if (!Path) return ERR_NullArgs;

   SimpleVector *vector = new_simplevector();
   if (!vector) return ERR_AllocMemory;

   vector->mPath.move_to(X, Y);
   vector->mPath.line_to(X+Width, Y);
   vector->mPath.line_to(X+Width, Y+Height);
   vector->mPath.line_to(X, Y+Height);
   vector->mPath.close_polygon();
   *Path = vector;
   return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
GeneratePath: Generates a path from an SVG path command sequence, or an empty path for custom configuration.

This function will generate a vector path from a sequence of fixed point coordinates and curve instructions.  The
resulting path can then be passed to vector functions that receive a Path parameter.  The path must be manually
deallocated with ~FreePath() once it is no longer required.

The Sequence is a string of points and instructions that define the path.  It is based on the SVG standard for the path
element 'd' attribute, but also provides some additional features that are present in the vector engine.  Commands are
case insensitive.

The following commands are supported:

<pre>
M: Move To
L: Line To
V: Vertical Line To
H: Horizontal Line To
Q: Quadratic Curve To
T: Quadratic Smooth Curve To
C: Curve To
S: Smooth Curve To
A: Arc
Z: Close Path
</pre>

The use of lower case characters will indicate that the provided coordinates are relative (based on the coordinate
of the previous command).

If the Sequence is NULL then an empty path resource will be generated.  This path will be suitable for passing
to path modifying functions such as ~MoveTo() and ~LineTo() for custom path generation.

-INPUT-
cstr Sequence: The command sequence to process.  If no sequence is specified then the path will be empty.
&ptr Path: A pointer variable that will receive the resulting path.

-ERRORS-
Okay
NullArgs
AllocMemory

*****************************************************************************/

static ERROR vecGeneratePath(CSTRING Sequence, APTR *Path)
{
   if (!Path) return ERR_NullArgs;

   ERROR error;
   struct PathCommand *paths;
   LONG total;

   if (!Sequence) {
      SimpleVector *vector = new_simplevector();
      if (vector) *Path = vector;
      else error = ERR_AllocMemory;
   }
   else if (!(error = read_path(&paths, &total, Sequence))) {
      SimpleVector *vector = new_simplevector();
      if (vector) {
         convert_to_aggpath(paths, total, &vector->mPath);
         *Path = vector;
      }
      else error = ERR_AllocMemory;
      FreeResource(paths);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
LineTo: Alter a path by setting a line-to command at the current vertex position.

This function alters a path by setting a line-to command at the current vertex position.  The index is then advanced by
one to the next vertex position.

-INPUT-
ptr Path: The vector path to modify.
double X: The line end point on the horizontal plane.
double Y: The line end point on the vertical plane.

*****************************************************************************/

static void vecLineTo(SimpleVector *Vector, DOUBLE X, DOUBLE Y)
{
   Vector->mPath.line_to(X, Y);
}

/*****************************************************************************

-FUNCTION-
ReadPainter: Parses a painter string into its colour, gradient and image values.

This function will parse an SVG style IRI into its equivalent internal lookup values.  The results can then be
processed for rendering a stroke or fill operation in the chosen style.

Colours can be referenced using one of three methods.  Colour names such as 'orange' and 'red' are accepted.  Hexadecimal
RGB values are supported in the format '#RRGGBBAA'.  Floating point RGB is supported as 'rgb(r,g,b,a)' whereby the
component values range between 0.0 and 1.0.

A Gradient, Image or Pattern can be referenced using the 'url(#name)' format, where the 'name' is a definition that has
been registered with the given Vector object.  If Vector is NULL then it will not be possible to find the reference.
Any failure to lookup a reference will be silently discarded.

-INPUT-
obj Vector: Optional.  Required if url() references are to be resolved.
cstr IRI: The IRI string to be translated.
struct(*DRGB) RGB: A colour will be returned here if specified in the IRI.
&obj(VectorGradient) Gradient: A VectorGradient will be returned here if specified in the IRI.
&obj(VectorImage) Image: A VectorImage will be returned here if specified in the IRI.
&obj(VectorPattern) Pattern: A VectorPattern will be returned here if specified in the IRI.

*****************************************************************************/

static void vecReadPainter(OBJECTPTR Vector, CSTRING IRI, struct DRGB *RGB, objVectorGradient **Gradient,
   objVectorImage **Image, objVectorPattern **Pattern)
{
   ULONG i;

   if (!IRI) return;

   if (RGB) RGB->Alpha = 0; // Nullify the colour
   if (Gradient) *Gradient = NULL;
   if (Image) *Image = NULL;
   if (Pattern) *Pattern = NULL;

   //FMSG("vecReadPainter()","%s", IRI);

next:
   while ((*IRI) and (*IRI <= 0x20)) IRI++;

   if (!StrCompare("url(", IRI, 4, 0)) {
      if (!Vector) return;
      objVectorScene *scene;

      if (Vector->ClassID IS ID_VECTOR) scene = ((objVector *)Vector)->Scene;
      else if (Vector->ClassID IS ID_VECTORSCENE) scene = (objVectorScene *)Vector;
      else return;

      IRI += 4;
      if (*IRI IS '#') {
         // Compute the hash identifier

         IRI++;
         char name[80];
         for (i=0; (IRI[i] != ')') and (IRI[i]) and (i < sizeof(name)-1); i++) name[i] = IRI[i];
         name[i] = 0;

         struct VectorDef *def;
         if (!VarGet(scene->Defs, name, &def, NULL)) {
            if (def->Object->ClassID IS ID_VECTORGRADIENT) {
               if (Gradient) *Gradient = (objVectorGradient *)def->Object;
            }
            else if (def->Object->ClassID IS ID_VECTORIMAGE) {
               if (Image) *Image = (objVectorImage *)def->Object;
            }
            else if (def->Object->ClassID IS ID_VECTORPATTERN) {
               if (Pattern) *Pattern = (objVectorPattern *)def->Object;
            }
            else LogErrorMsg("Vector definition '%s' (class $%.8x) not supported.", name, def->Object->ClassID);

            // Check for combinations
            if (IRI[i++] IS ')') {
               while ((IRI[i]) and (IRI[i] <= 0x20)) i++;
               if (IRI[i++] IS '+') {
                  IRI += i;
                  goto next;
               }
            }

            return;
         }

         LogErrorMsg("Failed to lookup IRI: %s", IRI);
      }
      else LogErrorMsg("Invalid IRI: %s", IRI);
   }
   else if (!StrCompare("rgb(", IRI, 4, 0)) {
      // Note that in some rare cases, RGB values are expressed in percentage terms, e.g. rgb(34.38%,0.23%,52%)
      IRI += 4;
      RGB->Red = StrToFloat(IRI) * (1.0 / 255.0);
      while ((*IRI) and (*IRI != ',')) {
         if (*IRI IS '%') RGB->Red = RGB->Red * (255.0 / 100.0);
         IRI++;
      }
      if (*IRI) IRI++;
      RGB->Green = StrToFloat(IRI) * (1.0 / 255.0);
      while ((*IRI) and (*IRI != ',')) {
         if (*IRI IS '%') RGB->Green = RGB->Green * (255.0 / 100.0);
         IRI++;
      }
      if (*IRI) IRI++;
      RGB->Blue = StrToFloat(IRI) * (1.0 / 255.0);
      while ((*IRI) and (*IRI != ',')) {
         if (*IRI IS '%') RGB->Blue = RGB->Blue * (255.0 / 100.0);
         IRI++;
      }
      if (*IRI) {
         IRI++;
         RGB->Alpha = StrToFloat(IRI) * (1.0 / 255.0);
         while (*IRI) {
            if (*IRI IS '%') RGB->Alpha = RGB->Alpha * (255.0 / 100.0);
            IRI++;
         }
         if (RGB->Alpha > 1) RGB->Alpha = 1;
         else if (RGB->Alpha < 0) RGB->Alpha = 0;
      }
      else if (RGB->Alpha <= 0) RGB->Alpha = 1.0; // Only set the alpha if it hasn't been set already (example: stroke-opacity)

      if (RGB->Red > 1) RGB->Red = 1;
      else if (RGB->Red < 0) RGB->Red = 0;

      if (RGB->Green > 1) RGB->Green = 1;
      else if (RGB->Green < 0) RGB->Green = 0;

      if (RGB->Blue > 1) RGB->Blue = 1;
      else if (RGB->Blue < 0) RGB->Blue = 0;
   }
   else if (*IRI IS '#') {
      struct RGB8 rgb;
      StrToColour(IRI, &rgb);
      RGB->Red   = (DOUBLE)rgb.Red   * (1.0 / 255.0);
      RGB->Green = (DOUBLE)rgb.Green * (1.0 / 255.0);
      RGB->Blue  = (DOUBLE)rgb.Blue  * (1.0 / 255.0);
      RGB->Alpha = (DOUBLE)rgb.Alpha * (1.0 / 255.0);
   }
   else {
      ULONG hash = StrHash(IRI, FALSE);
      for (WORD i=0; i < ARRAYSIZE(glNamedColours); i++) {
         if (glNamedColours[i].Hash IS hash) {
            RGB->Red   = (DOUBLE)glNamedColours[i].Red * (1.0/255.0);
            RGB->Green = (DOUBLE)glNamedColours[i].Green * (1.0/255.0);
            RGB->Blue  = (DOUBLE)glNamedColours[i].Blue * (1.0/255.0);
            RGB->Alpha = (DOUBLE)glNamedColours[i].Alpha * (1.0/255.0);
            return;
         }
      }

      LogErrorMsg("Failed to interpret colour: %s", IRI);
   }
}

/*****************************************************************************

-FUNCTION-
MoveTo: Alter a path by setting a move-to command at the current vertex position.

This function will set an move-to command at the current vertex.  It then increments the vertex position for the next
path command.

The move-to command is used to move the pen to a new coordinate without drawing a line.

-INPUT-
ptr Path: The vector path to modify.
double X: The horizontal end point for the command.
double Y: The vertical end point for the command.

*****************************************************************************/

static void vecMoveTo(SimpleVector *Vector, DOUBLE X, DOUBLE Y)
{
   Vector->mPath.move_to(X, Y);
}

/*****************************************************************************

-FUNCTION-
RewindPath: Resets the vertex seek position to zero.

Rewinding a path will reset the current vertex index to zero.  The next call to a vertex modification function such as
~LineTo() would result in the first vertex being modified.

If the referenced Path is empty, this function does nothing.

-INPUT-
ptr Path: The vector path to rewind.

*****************************************************************************/

static void vecRewindPath(SimpleVector *Vector)
{
   if (Vector) Vector->mPath.rewind(0);
}

/*****************************************************************************

-FUNCTION-
Smooth3: Alter a path by setting a smooth3 command at the current vertex position.

This function will set a quadratic bezier curve command at the current vertex.  It then increments the vertex position
for the next path command.

The control point from the previous curve is used as the control point for the new curve, hence the 'smooth'.

-INPUT-
ptr Path: The vector path to modify.
double X: The horizontal end point for the smooth3 command.
double Y: The vertical end point for the smooth3 command.

*****************************************************************************/

static void vecSmooth3(SimpleVector *Vector, DOUBLE X, DOUBLE Y)
{
   Vector->mPath.curve3(X, Y);
}

/*****************************************************************************

-FUNCTION-
Smooth4: Alter a path by setting a smooth4 command at the current vertex position.

This function will set a cubic bezier curve command at the current vertex.  It then increments the vertex position
for the next path command.

The control point from the previous curve will be used in addition to the CtrlX and CtrlY points, hence the
name 'smoothed curve'.

-INPUT-
ptr Path: The vector path to modify.
double CtrlX: Control point horizontal coordinate.
double CtrlY: Control point vertical coordinate.
double X: The horizontal end point for the smooth4 instruction.
double Y: The vertical end point for the smooth4 instruction.

*****************************************************************************/

static void vecSmooth4(SimpleVector *Vector, DOUBLE CtrlX, DOUBLE CtrlY, DOUBLE X, DOUBLE Y)
{
   Vector->mPath.curve4(CtrlX, CtrlY, X, Y);
}

/*****************************************************************************

-FUNCTION-
TranslatePath: Translates a path by (x,y)

This function will translate all vertices of a path by (X,Y).

-INPUT-
ptr Path: Pointer to a generated path.
double X: Translate the path horizontally by the given value.
double Y: Translate the path vertically by the given value.

-END-

*****************************************************************************/

static void vecTranslatePath(SimpleVector *Vector, DOUBLE X, DOUBLE Y)
{
   if (!Vector) return;

   Vector->mPath.translate_all_paths(X, Y);
}
