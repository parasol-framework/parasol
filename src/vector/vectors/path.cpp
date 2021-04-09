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

static void convert_to_aggpath(PathCommand *Paths, LONG TotalCommands, agg::path_storage *BasePath)
{
   if (!Paths) return;

   PathCommand dummy = { 0, 0, 0, 0, 0, 0 };
   PathCommand &lp = dummy;

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

static ERROR read_path(PathCommand **Path, LONG *Count, CSTRING Value)
{
   parasol::Log log(__FUNCTION__);
   PathCommand *path;

   // Get a rough estimate on how many array entries there will be (this sub-routine will over-estimate and then
   // we'll reduce the array size at the end).

   LONG guess = 0;
   for (LONG i=0; Value[i]; i++) {
      if (Value[i] IS 'z') guess++;
      else if ((Value[i] >= '0') and (Value[i] <= '9')) {
         while ((Value[i] >= '0') and (Value[i] <= '9')) i++;
         guess++;
         i--;
      }
   }

   //log.traceBranch("%d path points detected.", guess);

   if (AllocMemory(sizeof(PathCommand) * guess, MEM_DATA, &path, NULL)) return ERR_AllocMemory;

   if (Count) *Count = 0;
   if (Path)  *Path = NULL;
   LONG total = 0;
   UBYTE cmd = 0;
   while (*Value) {
      if ((*Value >= 'a') and (*Value <= 'z')) cmd = *Value++;
      else if ((*Value >= 'A') and (*Value <= 'Z')) cmd = *Value++;
      else if (((*Value >= '0') and (*Value <= '9')) or (*Value IS '-') or (*Value IS '+')); // Use the previous command
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
            Value = read_numseq(Value, &path[total].X2, &path[total].Y2, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'Q') path[total].Type = PE_QuadCurve;
            else path[total].Type = PE_QuadCurveRel;
            path[total].Curved = TRUE;
            break;

         case 'T': case 't': // Quadratic Smooth Curve To
            Value = read_numseq(Value, &path[total].X2, &path[total].Y2, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'T') path[total].Type = PE_QuadSmooth;
            else path[total].Type = PE_QuadSmoothRel;
             path[total].Curved = TRUE;
           break;

         case 'C': case 'c': // Curve To
            Value = read_numseq(Value, &path[total].X2, &path[total].Y2, &path[total].X3, &path[total].Y3, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'C') path[total].Type = PE_Curve;
            else path[total].Type = PE_CurveRel;
            path[total].Curved = TRUE;
            break;

         case 'S': case 's': // Smooth Curve To
            Value = read_numseq(Value, &path[total].X2, &path[total].Y2, &path[total].X, &path[total].Y, TAGEND);
            if (cmd IS 'S') path[total].Type = PE_Smooth;
            else path[total].Type = PE_SmoothRel;
            path[total].Curved = TRUE;
            break;

         case 'A': case 'a': { // Arc
            DOUBLE largearc, sweep;
            Value = read_numseq(Value, &path[total].X2, &path[total].Y2, &path[total].Angle, &largearc, &sweep, &path[total].X, &path[total].Y, TAGEND);
            path[total].LargeArc = F2T(largearc);
            path[total].Sweep = F2T(sweep);
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
            log.warning("Invalid path command '%c'", *Value);
            FreeResource(path);
            return ERR_Failed;
         }
      }
      if (++total >= guess) break;
   }

   //MSG("Processed %d actual path commands.", total);
   if (total >= 2) {
      if (total < guess-4) {
         // If our best guess was off by at least 4 entries, reduce the array size.
         ReallocMemory(path, sizeof(PathCommand) * total, &path, NULL);
      }

      if (Path) *Path = path;
      if (Count) *Count = total;
      return ERR_Okay;
   }
   else {
      FreeResource(path);
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
   if (Self->Commands) { FreeResource(Self->Commands); Self->Commands = NULL; }
   if (Self->CustomPath) { delete Self->CustomPath; Self->CustomPath = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_Init(objVectorPath *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Capacity < 1) return log.warning(ERR_OutOfRange);

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORPATH_NewObject(objVectorPath *Self, APTR Void)
{
   if (!AllocMemory(sizeof(PathCommand) * CAPACITY_CUSHION, MEM_DATA, &Self->Commands, NULL)) {
      Self->Capacity = CAPACITY_CUSHION;
      Self->GeneratePath = (void (*)(rkVector *))&generate_path;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
AddCommand: Add a command to the end of the path sequence.

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

static ERROR VECTORPATH_AddCommand(objVectorPath *Self, struct vpAddCommand *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Commands)) return log.warning(ERR_NullArgs);

   LONG total_cmds = Args->Size / sizeof(PathCommand);

   if ((total_cmds <= 0) or (total_cmds > 1000000)) return log.warning(ERR_Args);

   if (Self->TotalCommands + total_cmds > Self->Capacity) {
      PathCommand *new_list;
      LONG new_capacity = Self->Capacity + total_cmds + CAPACITY_CUSHION;
      if (AllocMemory(sizeof(PathCommand) * new_capacity, MEM_DATA|MEM_NO_CLEAR, &new_list, NULL)) {
         return ERR_AllocMemory;
      }

      CopyMemory(Self->Commands, new_list, Self->TotalCommands * sizeof(PathCommand));

      if (Self->Commands) FreeResource(Self->Commands);

      Self->Commands = new_list;
      Self->Capacity = new_capacity;
   }

   CopyMemory(Args->Commands, Self->Commands + Self->TotalCommands, total_cmds * sizeof(PathCommand));
   Self->TotalCommands += total_cmds;

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
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
OutOfRange

*****************************************************************************/

static ERROR VECTORPATH_GetCommand(objVectorPath *Self, struct vpGetCommand *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((Args->Index < 0) or (Args->Index >= Self->TotalCommands)) return log.warning(ERR_OutOfRange);

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
   parasol::Log log;

   if (!Args) return ERR_NullArgs;
   if ((Args->Index < 0) or (Args->Index > Self->TotalCommands-1)) return log.warning(ERR_OutOfRange);
   if (Self->TotalCommands < 1) return ERR_NothingDone;

   LONG total = Args->Total;
   if (Args->Index + total > Self->TotalCommands) {
      total = Self->TotalCommands - Args->Index;
   }

   CopyMemory(Self->Commands + Args->Index + total, Self->Commands + Args->Index, total * sizeof(PathCommand));
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
   parasol::Log log;

   if ((!Args) or (!Args->Command)) return ERR_NullArgs;
   if ((Args->Index < 0) or (Args->Index > Self->Capacity-1)) return log.warning(ERR_OutOfRange);

   LONG total_cmds = Args->Size / sizeof(PathCommand);
   if (Args->Index + total_cmds >= Self->Capacity) return log.warning(ERR_BufferOverflow);
   if (Args->Index + total_cmds > Self->TotalCommands) Self->TotalCommands = Args->Index + total_cmds;

   CopyMemory(Args->Command, Self->Commands + Args->Index, Args->Size);

   VECTORPATH_Flush(Self, NULL);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetCommandList: The fastest available mechanism for setting a series of path instructions.

Use SetCommandList to copy a series of path commands to a VectorPath object.  All existing commands
will be cleared as a result of this process.

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
   parasol::Log log;

   if ((!Args) or (!Args->Size)) return log.warning(ERR_NullArgs);

   if (!(Self->Head.Flags & NF_INITIALISED)) return log.warning(ERR_NotInitialised);

   LONG total_cmds = Args->Size / sizeof(PathCommand);
   if ((total_cmds < 0) or (total_cmds > 1000000)) return log.warning(ERR_Args);

   if (total_cmds > Self->Capacity) {
      PathCommand *new_list;
      LONG new_capacity = total_cmds + CAPACITY_CUSHION;
      if (AllocMemory(sizeof(PathCommand) * new_capacity, MEM_DATA|MEM_NO_CLEAR, &new_list, NULL)) {
         return ERR_AllocMemory;
      }

      if (Self->Commands) FreeResource(Self->Commands);

      Self->Commands = new_list;
      Self->Capacity = new_capacity;
   }

   CopyMemory(Args->Commands, Self->Commands, total_cmds * sizeof(PathCommand));
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
   parasol::Log log;

   if (Value < 1) return log.warning(ERR_InvalidValue);

   if (Value > Self->Capacity) {
      LONG new_capacity = Value + CAPACITY_CUSHION;
      if (Self->TotalCommands > 0) { // Preserve existing commands?
         if (!ReallocMemory(Self->Commands, sizeof(PathCommand) * new_capacity, &Self->Commands, NULL)) {
            Self->Capacity = new_capacity;
            return ERR_Okay;
         }
         else return ERR_AllocMemory;
      }
      else {
         PathCommand *new_list;
         if (!AllocMemory(sizeof(PathCommand) * new_capacity, MEM_DATA|MEM_NO_CLEAR, &new_list, NULL)) {
            if (Self->Commands) FreeResource(Self->Commands);
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

static ERROR VECTORPATH_GET_Commands(objVectorPath *Self, PathCommand **Value, LONG *Elements)
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
      FreeResource(Self->Commands);
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
   parasol::Log log;
   if ((Value < 0) or (Value > Self->Capacity)) return log.warning(ERR_OutOfRange);
   Self->TotalCommands = Value;
   return ERR_Okay;
}

//****************************************************************************

static const FieldArray clPathFields[] = {
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
