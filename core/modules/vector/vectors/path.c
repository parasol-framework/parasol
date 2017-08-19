/*****************************************************************************

-CLASS-
VectorPath: Extends the Vector class with support for generating custom paths.

VectorPath provides support for parsing SVG styled path strings.

-END-

*****************************************************************************/

#define CAPACITY_CUSHION 40

//****************************************************************************

static void generate_path(objVectorPath *Vector)
{
   if (!Vector->Commands) return;

   convert_to_aggpath(Vector->Commands, Vector->TotalCommands, Vector->BasePath);
}

//****************************************************************************

static void convert_to_aggpath(struct PathCommand *Paths, LONG TotalCommands, agg::path_storage *BasePath)
{
   if (!Paths) return;

   struct PathCommand dummy = { 0, 0, 0, 0, 0, 0 };
   struct PathCommand &lp = dummy;

   auto bp = BasePath;
   for (LONG i=0; i < TotalCommands; i++) {
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
            bp->curve4(path.Curve.X1, path.Curve.Y1, path.Curve.X2, path.Curve.Y2, path.AbsX, path.AbsY);
            break;

         case PE_CurveRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->curve4(path.Curve.X1+lp.AbsX, path.Curve.Y1+lp.AbsY, path.Curve.X2+lp.AbsX, path.Curve.Y2+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_Smooth: // Simplified curve3/4 with one control inherited from previous vertex
            path.AbsX = path.X;
            path.AbsY = path.Y;
            if (!lp.Curved) bp->curve3(path.Smooth.X, path.Smooth.Y, path.AbsX, path.AbsY);
            else bp->curve4(path.Smooth.X, path.Smooth.Y, path.AbsX, path.AbsY);
            break;

         case PE_SmoothRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            if (!lp.Curved) bp->curve3(path.Smooth.X+lp.AbsX, path.Smooth.Y+lp.AbsY, path.AbsX, path.AbsY);
            else bp->curve4(path.Smooth.X+lp.AbsX, path.Smooth.Y+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_QuadCurve:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->curve3(path.QuadCurve.X, path.QuadCurve.Y, path.AbsX, path.AbsY);
            break;

         case PE_QuadCurveRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->curve3(path.QuadCurve.X+lp.AbsX, path.QuadCurve.Y+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_QuadSmooth:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->curve4(path.QuadSmooth.X, path.QuadSmooth.Y, path.AbsX, path.AbsY);
            break;

         case PE_QuadSmoothRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->curve4(path.QuadSmooth.X+lp.AbsX, path.QuadSmooth.Y+lp.AbsY, path.AbsX, path.AbsY);
            break;

         case PE_Arc:
            path.AbsX = path.X;
            path.AbsY = path.Y;
            bp->arc_to(path.Arc.RX, path.Arc.RY, path.Arc.Angle, path.Arc.LargeArc, path.Arc.Sweep, path.AbsX, path.AbsY);
            break;

         case PE_ArcRel:
            path.AbsX = lp.AbsX + path.X;
            path.AbsY = lp.AbsY + path.Y;
            bp->arc_to(path.Arc.RX+lp.AbsX, path.Arc.RY+lp.AbsY, path.Arc.Angle, path.Arc.LargeArc, path.Arc.Sweep, path.AbsX, path.AbsY);
            break;

         case PE_ClosePath:
            bp->close_polygon();
            break;
      }

      lp = path;
   }
}

//****************************************************************************

static ERROR read_path(struct PathCommand **Path, LONG *Count, CSTRING Value)
{
   struct PathCommand *path;

   // Get a rough estimate on how many array entries there will be (this sub-routine will over-estimate and then
   // we'll reduce the array size at the end).

   LONG guess = 0;
   for (LONG i=0; Value[i]; i++) {
      if (Value[i] IS 'z') guess++;
      else if ((Value[i] >= '0') AND (Value[i] <= '9')) {
         while ((Value[i] >= '0') AND (Value[i] <= '9')) i++;
         guess++;
         i--;
      }
   }

   //FMSG("read_path()","%d path points detected.", guess);

   if (AllocMemory(sizeof(struct PathCommand) * guess, MEM_DATA, &path, NULL)) return ERR_AllocMemory;

   if (Count) *Count = 0;
   if (Path)  *Path = NULL;
   LONG total = 0;
   UBYTE cmd = 0;
   while (*Value) {
      if ((*Value >= 'a') AND (*Value <= 'z')) cmd = *Value++;
      else if ((*Value >= 'A') AND (*Value <= 'Z')) cmd = *Value++;
      else if (((*Value >= '0') AND (*Value <= '9')) OR (*Value IS '-') OR (*Value IS '+')); // Use the previous command
      else { Value++; continue; }

      switch (cmd) {
         case 'M': case 'm': // MoveTo
            Value = read_numseq(Value, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'M') {
               path[total].Type = PE_Move;
               cmd = 'L'; // This is because the SVG standard requires that sequential coordinate pairs will be interpreted as line-to commands.
            }
            else {
               path[total].Type = PE_MoveRel;
               cmd = 'l';
            }
            path[total].Curved = FALSE;
            break;

         case 'L': case 'l': // LineTo
            Value = read_numseq(Value, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'L') path[total].Type = PE_Line;
            else path[total].Type = PE_LineRel;
            path[total].Curved = FALSE;
            break;

         case 'V': case 'v': // Vertical LineTo
            Value = read_numseq(Value, &path[total].Y, TAGEND);
            if (cmd IS 'V') path[total].Type = PE_VLine;
            else path[total].Type = PE_VLineRel;
            path[total].Curved = FALSE;
            break;

         case 'H': case 'h': // Horizontal LineTo
            Value = read_numseq(Value, &path[total].X, TAGEND);
            if (cmd IS 'H') path[total].Type = PE_HLine;
            else path[total].Type = PE_LineRel;
            path[total].Curved = FALSE;
            break;

         case 'Q': case 'q': // Quadratic Curve To
            Value = read_numseq(Value, &path[total].QuadCurve.X, &path[total].QuadCurve.Y, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'Q') path[total].Type = PE_QuadCurve;
            else path[total].Type = PE_QuadCurveRel;
            path[total].Curved = TRUE;
            break;

         case 'T': case 't': // Quadratic Smooth Curve To
            Value = read_numseq(Value, &path[total].QuadSmooth.X, &path[total].QuadSmooth.Y, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'T') path[total].Type = PE_QuadSmooth;
            else path[total].Type = PE_QuadSmoothRel;
             path[total].Curved = TRUE;
           break;

         case 'C': case 'c': // Curve To
            Value = read_numseq(Value, &path[total].Curve.X1, &path[total].Curve.Y1, &path[total].Curve.X2, &path[total].Curve.Y2, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'C') path[total].Type = PE_Curve;
            else path[total].Type = PE_CurveRel;
            path[total].Curved = TRUE;
            break;

         case 'S': case 's': // Smooth Curve To
            Value = read_numseq(Value, &path[total].Smooth.X, &path[total].Smooth.Y, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'S') path[total].Type = PE_Smooth;
            else path[total].Type = PE_SmoothRel;
            path[total].Curved = TRUE;
            break;

         case 'A': case 'a': { // Arc
            DOUBLE largearc, sweep;
            Value = read_numseq(Value, &path[total].Arc.RX, &path[total].Arc.RY, &path[total].Arc.Angle, &largearc, &sweep, &path[total].X, &path[total].Y, TAGEND);
            path[total].Arc.LargeArc = F2T(largearc);
            path[total].Arc.Sweep = F2T(sweep);
            if (cmd IS 'A') path[total].Type = PE_Arc;
            else path[total].Type = PE_ArcRel;
            path[total].Curved = TRUE;
            break;
         }

         // W3C: When a subpath ends in a "closepath," it differs in behaviour from what happens when "manually" closing
         // a subpath via a "lineto" command in how ‘stroke-linejoin’ and ‘stroke-linecap’ are implemented. With
         // "closepath", the end of the final segment of the subpath is "joined" with the start of the initial segment
         // of the subpath using the current value of ‘stroke-linejoin’. If you instead "manually" close the subpath
         // via a "lineto" command, the start of the first segment and the end of the last segment are not joined but
         // instead are each capped using the current value of ‘stroke-linecap’. At the end of the command, the new
         // current point is set to the initial point of the current subpath.

         case 'Z': case 'z': { // Close Path
            path[total].Type = PE_ClosePath;
            path[total].Curved = FALSE;
            break;
         }

         default: {
            LogErrorMsg("Invalid path command '%c'", *Value);
            FreeMemory(path);
            return ERR_Failed;
         }
      }
      if (++total >= guess) break;
   }

   //MSG("Processed %d actual path commands.", total);
   if (total >= 2) {
      if (total < guess-4) {
         // If our best guess was off by at least 4 entries, reduce the array size.
         ReallocMemory(path, sizeof(struct PathCommand) * total, &path, NULL);
      }

      if (Path) *Path = path;
      if (Count) *Count = total;
      return ERR_Okay;
   }
   else {
      FreeMemory(path);
      return ERR_Failed;
   }
}

//****************************************************************************

static ERROR VECTORPATH_Clear(objVectorPath *Self, APTR Void)
{
   Self->TotalCommands = 0;
   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Flush(objVectorPath *Self, APTR Void)
{
//   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   reset_path(Self);
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Free(objVectorPath *Self, APTR Void)
{
   if (Self->Commands) { FreeMemory(Self->Commands); Self->Commands = NULL; }
   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Init(objVectorPath *Self, APTR Void)
{
   if (Self->Capacity < 1) return PostError(ERR_OutOfRange);

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_NewObject(objVectorPath *Self, APTR Void)
{
   if (!AllocMemory(VECTORPATH_CMD_SIZE * CAPACITY_CUSHION, MEM_DATA, &Self->Commands, NULL)) {
      Self->Capacity = CAPACITY_CUSHION;
      Self->GeneratePath = (void (*)(struct rkVector *))&generate_path;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
AddCommand: Add a command to the end of the path sequence.

TBA

-INPUT-
buf(struct(*PathCommand)) Command: Array of commands to add to the path.
bufsize Size: The size of the Command buffer, in bytes.

-RESULT-
Okay

*****************************************************************************/

static ERROR VECTORPATH_AddCommand(objVectorPath *Self, struct vpAddCommand *Args)
{
   return ERR_NoSupport;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR VECTORPATH_GetCommand(objVectorPath *Self, struct vpGetCommand *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if ((Args->Index < 0) OR (Args->Index >= Self->TotalCommands)) return PostError(ERR_OutOfRange);

   Args->Command = &Self->Commands[Args->Index];
   return ERR_Okay;
}

/*****************************************************************************

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

static ERROR VECTORPATH_RemoveCommand(objVectorPath *Self, struct vpRemoveCommand *Args)
{
   if (!Args) return ERR_NullArgs;
   if ((Args->Index < 0) OR (Args->Index > Self->TotalCommands-1)) return PostError(ERR_OutOfRange);
   if (Self->TotalCommands < 1) return ERR_NothingDone;

   LONG total = Args->Total;
   if (Args->Index + total > Self->TotalCommands) {
      total = Self->TotalCommands - Args->Index;
   }

   CopyMemory(Self->Commands + Args->Index + total, Self->Commands + Args->Index, total * VECTORPATH_CMD_SIZE);
   Self->TotalCommands -= total;

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetCommand: Copies one or more commands into an existing path.

Use SetCommand to copy one or more commands into an existing path.  This method cannot be used to expand the path
beyond its #Capacity.

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

static ERROR VECTORPATH_SetCommand(objVectorPath *Self, struct vpSetCommand *Args)
{
   if ((!Args) OR (!Args->Command)) return ERR_NullArgs;
   if ((Args->Index < 0) OR (Args->Index > Self->Capacity-1)) return PostError(ERR_OutOfRange);

   LONG total_cmds = Args->Size / VECTORPATH_CMD_SIZE;
   if (Args->Index + total_cmds >= Self->Capacity) return PostError(ERR_BufferOverflow);
   if (Args->Index + total_cmds > Self->TotalCommands) Self->TotalCommands = Args->Index + total_cmds;

   CopyMemory(Args->Command, Self->Commands + Args->Index, Args->Size);

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetCommandList: The fastest available mechanism for setting a series of path instructions.

Use SetCommandList to copy a series of path commands to a VectorPath object, overwriting any existing instructions
in the process.

-INPUT-
buf(ptr) Commands: An array of path command structures.
bufsize Size: The byte size of the Commands buffer.

-RESULT-
Okay
NullArgs
Args
AllocMemory

*****************************************************************************/

static ERROR VECTORPATH_SetCommandList(objVectorPath *Self, struct vpSetCommandList *Args)
{
   if ((!Args) OR (!Args->Size)) return PostError(ERR_NullArgs);

   if (!(Self->Head.Flags & NF_INITIALISED)) return PostError(ERR_NotInitialised);

   LONG total_cmds = Args->Size / VECTORPATH_CMD_SIZE;
   if ((total_cmds < 0) OR (total_cmds > 1000000)) return PostError(ERR_Args);

   if (total_cmds > Self->Capacity) {
      struct PathCommand *new_list;
      LONG new_capacity = total_cmds + CAPACITY_CUSHION;
      if (AllocMemory(VECTORPATH_CMD_SIZE * new_capacity, MEM_DATA|MEM_NO_CLEAR, &new_list, NULL)) {
         return ERR_AllocMemory;
      }

      if (Self->Commands) FreeMemory(Self->Commands);

      Self->TotalCommands = 0;
      Self->Capacity = 0;
      Self->Commands = new_list;
      Self->Capacity = new_capacity;
   }

   CopyMemory(Args->Commands, Self->Commands, total_cmds * VECTORPATH_CMD_SIZE);
   Self->TotalCommands = total_cmds;

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Capacity: The maximum number of commands that can be supported before the internal buffer requires reallocation.

The maximum number of commands that can be supported before the internal buffer requires reallocation.

*****************************************************************************/

static ERROR VECTORPATH_GET_Capacity(objVectorPath *Self, LONG *Value)
{
   *Value = Self->Capacity;
   return ERR_Okay;
}

static ERROR VECTORPATH_SET_Capacity(objVectorPath *Self, LONG Value)
{
   if (Value < 1) return PostError(ERR_InvalidValue);

   if (Value > Self->Capacity) {
      LONG new_capacity = Value + CAPACITY_CUSHION;
      if (Self->TotalCommands > 0) { // Preserve existing commands?
         if (!ReallocMemory(Self->Commands, VECTORPATH_CMD_SIZE * new_capacity, &Self->Commands, NULL)) {
            Self->Capacity = new_capacity;
            return ERR_Okay;
         }
         else return ERR_AllocMemory;
      }
      else {
         struct PathCommand *new_list;
         if (!AllocMemory(VECTORPATH_CMD_SIZE * new_capacity, MEM_DATA|MEM_NO_CLEAR, &new_list, NULL)) {
            if (Self->Commands) FreeMemory(Self->Commands);
            Self->Commands = new_list;
            Self->Capacity = new_capacity;
            return ERR_Okay;
         }
         else return ERR_AllocMemory;
      }
   }
   else return ERR_NothingDone;
}

/*****************************************************************************
-FIELD-
Commands: Direct pointer to the PathCommand array.

Read the Commands field to obtain a direct pointer to the PathCommand array.  This will allow the path to be modified
directly.  After making changes to the path, call #Flush() to register the changes for the next redraw.

*****************************************************************************/

static ERROR VECTORPATH_GET_Commands(objVectorPath *Self, struct PathCommand **Value, LONG *Elements)
{
   *Value = Self->Commands;
   *Elements = Self->TotalCommands;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
PathLength: Calibrates the user agent's distance-along-a-path calculations with that of the author.

The author's computation of the total length of the path, in user units. This value is used to calibrate the user
agent's own distance-along-a-path calculations with that of the author. The user agent will scale all
distance-along-a-path computations by the ratio of PathLength to the user agent's own computed value for total path
length.  This feature potentially affects calculations for text on a path, motion animation and various stroke
operations.

*****************************************************************************/

static ERROR VECTORPATH_GET_PathLength(objVectorPath *Self, LONG *Value)
{
   *Value = Self->PathLength;
   return ERR_Okay;
}

static ERROR VECTORPATH_SET_PathLength(objVectorPath *Self, LONG Value)
{
   if (Value >= 0) {
      Self->PathLength = Value;
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*****************************************************************************
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

static ERROR VECTORPATH_SET_Sequence(objVectorPath *Self, CSTRING Value)
{
   if (Self->Commands) {
      FreeMemory(Self->Commands);
      Self->Commands = NULL;
      Self->TotalCommands = 0;
   }

   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }

   ERROR error = ERR_Okay;
   if (Value) error = read_path(&Self->Commands, &Self->TotalCommands, Value);
   reset_path(Self);
   return error;
}

/*****************************************************************************
-FIELD-
TotalCommands: The total number of points defined in the path sequence.

The total number of points defined in the path #Sequence is reflected in this field.  Modifying the total directly is
permitted if the #Commands array is large enough to cover the new value.
-END-
*****************************************************************************/

static ERROR VECTORPATH_GET_TotalCommands(objVectorPath *Self, LONG *Value)
{
   *Value = Self->TotalCommands;
   return ERR_Okay;
}

static ERROR VECTORPATH_SET_TotalCommands(objVectorPath *Self, LONG Value)
{
   if ((Value < 0) OR (Value > Self->Capacity)) return PostError(ERR_OutOfRange);
   Self->TotalCommands = Value;
   return ERR_Okay;
}

//****************************************************************************

static const struct FieldArray clPathFields[] = {
   { "Sequence",      FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, (APTR)VECTOR_GET_Sequence, (APTR)VECTORPATH_SET_Sequence },
   { "TotalCommands", FDF_VIRTUAL|FDF_LONG|FDF_RW,   0, (APTR)VECTORPATH_GET_TotalCommands, (APTR)VECTORPATH_SET_TotalCommands },
   { "PathLength",    FDF_VIRTUAL|FDF_LONG|FDF_RW,   0, (APTR)VECTORPATH_GET_PathLength, (APTR)VECTORPATH_SET_PathLength },
   { "Capacity",      FDF_VIRTUAL|FDF_LONG|FDF_RW,   0, (APTR)VECTORPATH_GET_Capacity, (APTR)VECTORPATH_SET_Capacity },
   { "Commands",      FDF_VIRTUAL|FDF_ARRAY|FDF_STRUCT|FDF_R, (MAXINT)"PathCommand", (APTR)VECTORPATH_GET_Commands, NULL },
   END_FIELD
};

#include "path_def.c"

//****************************************************************************

static ERROR init_path(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorPath,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORPATH,
      FID_Name|TSTR,         "VectorPath",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clVectorPathActions,
      FID_Methods|TARRAY,    clVectorPathMethods,
      FID_Fields|TARRAY,     clPathFields,
      FID_Size|TLONG,        sizeof(objVectorPath),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}
