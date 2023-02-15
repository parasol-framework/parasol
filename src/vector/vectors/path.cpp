/*********************************************************************************************************************

-CLASS-
VectorPath: Extends the Vector class with support for generating custom paths.

VectorPath provides support for parsing SVG styled path strings.

-END-

*****************************************************************************/

//****************************************************************************

static void generate_path(extVectorPath *Vector)
{
   // TODO: We may be able to drop our internal PathCommand type in favour of agg:path_storage (and
   // extend it if necessary).
   convert_to_aggpath(Vector->Commands, &Vector->BasePath);
   bounding_rect_single(Vector->BasePath, 0, &Vector->BX1, &Vector->BY1, &Vector->BX2, &Vector->BY2);
}

//****************************************************************************

void convert_to_aggpath(std::vector<PathCommand> &Paths, agg::path_storage *BasePath)
{
   PathCommand dummy = { 0, 0, 0, 0, 0, 0 };
   PathCommand &lp = dummy;

   auto bp = BasePath;
   for (size_t i=0; i < Paths.size(); i++) {
      auto path = Paths[i];

      switch (path.Type) {
         case PE_Move:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->move_to(path.AbsX, path.AbsY);
            break;

         case PE_MoveRel:
            path.AbsX = path.X + lp.AbsX;
            path.AbsY = path.Y + lp.AbsY;
            bp->move_to(path.AbsX, path.AbsY);
            break;

         case PE_Line:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->line_to(path.AbsX, path.AbsY);
            break;

         case PE_LineRel:
            path.AbsX = path.X + lp.AbsX;
            path.AbsY = path.Y + lp.AbsY;
            bp->line_to(path.AbsX, path.AbsY);
            break;

         case PE_HLine:
            path.AbsX = path.X;
            path.AbsY = lp.AbsY;
            bp->line_to(path.AbsX, path.AbsY);
            break;

         case PE_HLineRel:
            path.AbsX = path.X + lp.AbsX;
            path.AbsY = lp.AbsY;
            bp->line_to(path.AbsX, path.AbsY);
            break;

         case PE_VLine:
            path.AbsX = lp.AbsX;
            path.AbsY = path.Y;
            bp->line_to(path.AbsX, path.AbsY);
            break;

         case PE_VLineRel:
            path.AbsX = lp.AbsX;
            path.AbsY = path.Y + lp.AbsY;
            bp->line_to(path.AbsX, path.AbsY);
            break;

         case PE_Curve: // curve4()
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->curve4(path.X2, path.Y2, path.X3, path.Y3, path.AbsX, path.AbsY);
            break;

         case PE_CurveRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->curve4(path.X2+lp.AbsX, path.Y2+lp.AbsY, path.X3+lp.AbsX, path.Y3+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_Smooth: // Simplified curve3/4 with one control inherited from previous vertex
            path.AbsX = path.X;
            path.AbsY = path.Y;
            if (!lp.Curved) bp->curve3(path.X2, path.Y2, path.AbsX, path.AbsY);
            else bp->curve4(path.X2, path.Y2, path.AbsX, path.AbsY);
            break;

         case PE_SmoothRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            if (!lp.Curved) bp->curve3(path.X2+lp.AbsX, path.Y2+lp.AbsY, path.AbsX, path.AbsY);
            else bp->curve4(path.X2+lp.AbsX, path.Y2+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_QuadCurve:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->curve3(path.X2, path.Y2, path.AbsX, path.AbsY);
            break;

         case PE_QuadCurveRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->curve3(path.X2+lp.AbsX, path.Y2+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_QuadSmooth: // Inherits a control from previous vertex
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->curve4(path.X2, path.Y2, path.AbsX, path.AbsY);
            break;

         case PE_QuadSmoothRel: // Inherits a control from previous vertex
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->curve4(path.X2+lp.AbsX, path.Y2+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_Arc:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->arc_to(path.X2, path.Y2, path.Angle, path.LargeArc, path.Sweep, path.AbsX, path.AbsY);
            break;

         case PE_ArcRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->arc_to(path.X2+lp.AbsX, path.Y2+lp.AbsY, path.Angle, path.LargeArc, path.Sweep, path.AbsX, path.AbsY);
            break;

         case PE_ClosePath:
            bp->close_polygon();
            break;
      }

      lp = path;
   }
}

//****************************************************************************

static ERROR VECTORPATH_Clear(extVectorPath *Self, APTR Void)
{
   Self->Commands.clear();
   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Flush(extVectorPath *Self, APTR Void)
{
//   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Free(extVectorPath *Self, APTR Void)
{
   Self->Commands.~vector();
   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Init(extVectorPath *Self, APTR Void)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_NewObject(extVectorPath *Self, APTR Void)
{
   new(&Self->Commands) std::vector<PathCommand>;
   Self->GeneratePath = (void (*)(extVector *))&generate_path;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
AddCommand: Add one or more commands to the end of the path sequence.

This method will add a series of commands to the end of a Vector's existing path sequence.  The commands must be
provided as a sequential array.  No checks will be performed to confirm the validity of the sequence.

Calling this method will also result in the path being recomputed for the next redraw.

-INPUT-
buf(struct(*PathCommand)) Commands: Array of commands to add to the path.
bufsize Size: The size of the Command buffer, in bytes.

-RESULT-
Okay
NullArgs

*****************************************************************************/

static ERROR VECTORPATH_AddCommand(extVectorPath *Self, struct vpAddCommand *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Commands)) return log.warning(ERR_NullArgs);

   const LONG total_cmds = Args->Size / sizeof(PathCommand);

   if ((total_cmds <= 0) or (total_cmds > 1000000)) return log.warning(ERR_Args);

   PathCommand *list = Args->Commands;
   for (LONG i=0; i < total_cmds; i++) {
      Self->Commands.push_back(list[i]);
   }

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
GetCommand: Retrieve a specific command from the path sequence.

Calling GetCommand will return a direct pointer to the command identified at Index.  The pointer will remain valid
for as long as the VectorPath is not modified.

-INPUT-
int Index: The index of the command to retrieve.
&struct(*PathCommand) Command: The requested command will be returned in this parameter.

-RESULT-
Okay
NullArgs
OutOfRange

*****************************************************************************/

static ERROR VECTORPATH_GetCommand(extVectorPath *Self, struct vpGetCommand *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or ((size_t)Args->Index >= Self->Commands.size())) return log.warning(ERR_OutOfRange);

   Args->Command = &Self->Commands[Args->Index];
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
RemoveCommand: Remove at least one command from the path sequence.

This method will remove a series of commands from the current path, starting at the given Index.  The total number of
commands to remove is indicated by the Total parameter.

-INPUT-
int Index: The index of the command to remove.
int Total: The total number of commands to remove, starting from the given Index.

-RESULT-
Okay
NullArgs
OutOfRange
NothingDone

*****************************************************************************/

static ERROR VECTORPATH_RemoveCommand(extVectorPath *Self, struct vpRemoveCommand *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;
   if ((Args->Index < 0) or ((size_t)Args->Index > Self->Commands.size()-1)) return log.warning(ERR_OutOfRange);
   if (Self->Commands.empty()) return ERR_NothingDone;

   auto first = Self->Commands.begin() + Args->Index;
   auto last = first + Args->Total;
   Self->Commands.erase(first, last);

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetCommand: Copies one or more commands into an existing path.

Use SetCommand to copy one or more commands into an existing path.

-INPUT-
int Index: The index of the command that is to be set.
buf(struct(*PathCommand)) Command: An array of commands to set in the path.
bufsize Size: The size of the Command buffer, in bytes.

-RESULT-
Okay
NullArgs
OutOfRange
BufferOverflow

*****************************************************************************/

static ERROR VECTORPATH_SetCommand(extVectorPath *Self, struct vpSetCommand *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Command)) return ERR_NullArgs;
   if (Args->Index < 0) return log.warning(ERR_OutOfRange);

   const LONG total_cmds = Args->Size / sizeof(PathCommand);
   if ((size_t)Args->Index + total_cmds > Self->Commands.size()) Self->Commands.resize(Args->Index + total_cmds);

   PathCommand *list = Args->Command;
   for (LONG i=0; i < total_cmds; i++) {
      CopyMemory(&list[i], &Self->Commands[Args->Index + i], sizeof(PathCommand));
   }

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
SetCommandList: The fastest available mechanism for setting a series of path instructions.

Use SetCommandList to copy a series of path commands to a VectorPath object.  All existing commands
will be cleared as a result of this process.

NOTE: This method is not compatible with Fluid calls.

-INPUT-
buf(ptr) Commands: An array of path command structures.
bufsize Size: The byte size of the Commands buffer.

-RESULT-
Okay
NullArgs
NotInitialised
Args

*****************************************************************************/

static ERROR VECTORPATH_SetCommandList(extVectorPath *Self, struct vpSetCommandList *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Size)) return log.warning(ERR_NullArgs);

   if (!Self->initialised()) return log.warning(ERR_NotInitialised);

   const LONG total_cmds = Args->Size / sizeof(PathCommand);
   if ((total_cmds < 0) or (total_cmds > 1000000)) return log.warning(ERR_Args);

   Self->Commands.clear();

   auto list = (PathCommand *)Args->Commands;
   for (LONG i=0; i < total_cmds; i++) {
      Self->Commands.push_back(list[i]);
   }

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Commands: Direct pointer to the PathCommand array.

Read the Commands field to obtain a direct pointer to the PathCommand array.  This will allow the path to be modified
directly.  After making changes to the path, call #Flush() to register the changes for the next redraw.

*****************************************************************************/

static ERROR VECTORPATH_GET_Commands(extVectorPath *Self, PathCommand **Value, LONG *Elements)
{
   *Value = Self->Commands.data();
   *Elements = Self->Commands.size();
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

*****************************************************************************/

static ERROR VECTORPATH_GET_PathLength(extVectorPath *Self, LONG *Value)
{
   *Value = Self->PathLength;
   return ERR_Okay;
}

static ERROR VECTORPATH_SET_PathLength(extVectorPath *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PathLength = Value;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Sequence: A sequence of points and instructions that will define the path.

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

To terminate a path without joining it to the first coordinate, omit the 'Z' from the end of the sequence.

*****************************************************************************/

static ERROR VECTORPATH_SET_Sequence(extVectorPath *Self, CSTRING Value)
{
   Self->Commands.clear();
   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }

   ERROR error = ERR_Okay;
   if (Value) error = read_path(Self->Commands, Value);
   reset_path(Self);
   return error;
}

/*********************************************************************************************************************
-FIELD-
TotalCommands: The total number of points defined in the path sequence.

The total number of points defined in the path #Sequence is reflected in this field.  Modifying the total directly is
permitted, although this should be used for shrinking the list because expansion will create uninitialised command entries.
-END-
*****************************************************************************/

static ERROR VECTORPATH_GET_TotalCommands(extVectorPath *Self, LONG *Value)
{
   *Value = Self->Commands.size();
   return ERR_Okay;
}

static ERROR VECTORPATH_SET_TotalCommands(extVectorPath *Self, LONG Value)
{
   parasol::Log log;
   if (Value < 0) return log.warning(ERR_OutOfRange);
   Self->Commands.resize(Value);
   return ERR_Okay;
}

//****************************************************************************

static const FieldArray clPathFields[] = {
   { "Sequence",      FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, (APTR)VECTOR_GET_Sequence, (APTR)VECTORPATH_SET_Sequence },
   { "TotalCommands", FDF_VIRTUAL|FDF_LONG|FDF_RW,   0, (APTR)VECTORPATH_GET_TotalCommands, (APTR)VECTORPATH_SET_TotalCommands },
   { "PathLength",    FDF_VIRTUAL|FDF_LONG|FDF_RW,   0, (APTR)VECTORPATH_GET_PathLength, (APTR)VECTORPATH_SET_PathLength },
   { "Commands",      FDF_VIRTUAL|FDF_ARRAY|FDF_STRUCT|FDF_R, (MAXINT)"PathCommand", (APTR)VECTORPATH_GET_Commands, NULL },
   END_FIELD
};

#include "path_def.c"

//****************************************************************************

static ERROR init_path(void)
{
   clVectorPath = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::SubClassID(ID_VECTORPATH),
      fl::Name("VectorPath"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clVectorPathActions),
      fl::Methods(clVectorPathMethods),
      fl::Fields(clPathFields),
      fl::Size(sizeof(extVectorPath)),
      fl::Path(MOD_PATH));

   return clVectorPath ? ERR_Okay : ERR_AddClass;
}
