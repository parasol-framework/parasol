/*********************************************************************************************************************

-CLASS-
VectorWave: Extends the Vector class with support for sine wave based paths.

The VectorWave class provides functionality for generating paths based on sine waves.  This feature is not part of the
SVG standard and therefore should not be used in cases where SVG compliance is a strict requirement.

The sine wave will be generated within a rectangular region at (#X,#Y) with size (#Width,#Height).  The horizontal
center-line within the rectangle will dictate the orientation of the sine wave, and the path vertices are generated
on a left-to-right basis.

Waves can be used in Parasol's SVG implementation by using the &lt;parasol:wave/&gt; element.

-END-

*****************************************************************************/

class extVectorWave : public extVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORWAVE;
   static constexpr CSTRING CLASS_NAME = "VectorWave";
   using create = parasol::Create<extVectorWave>;

   DOUBLE wX, wY;
   DOUBLE wWidth, wHeight;
   DOUBLE wAmplitude;
   DOUBLE wFrequency;
   DOUBLE wDecay;
   DOUBLE wDegree;
   DOUBLE wThickness;
   LONG   wDimensions;
   UBYTE  wClose;
   UBYTE  wStyle;
};

//********************************************************************************************************************

static void generate_wave(extVectorWave *Vector)
{
   DOUBLE ox = Vector->wX, oy = Vector->wY;
   DOUBLE width = Vector->wWidth, height = Vector->wHeight;

   if (Vector->wDimensions & DMF_RELATIVE_X) ox *= get_parent_width(Vector);
   if (Vector->wDimensions & DMF_RELATIVE_Y) oy *= get_parent_height(Vector);
   if (Vector->wDimensions & DMF_RELATIVE_WIDTH) width *= get_parent_width(Vector);
   if (Vector->wDimensions & DMF_RELATIVE_HEIGHT) height *= get_parent_height(Vector);

   DOUBLE decay;
   if (Vector->wDecay IS 0) decay = 0.00000001;
   else if (Vector->wDecay >= 0) decay = 360 * Vector->wDecay;
   else decay = 360 * -Vector->wDecay;

   const DOUBLE amp = (height * 0.5) * Vector->wAmplitude;
   const DOUBLE scale = 1.0 / Vector->Transform.scale(); // Essential for smooth curves when scale > 1.0

   DOUBLE x = 0, y = sin(DEG2RAD * Vector->wDegree) * amp + (height * 0.5);
   if (Vector->Transition) apply_transition_xy(Vector->Transition, 0, &x, &y);

   if ((!Vector->wClose) or (Vector->wThickness > 0)) {
      Vector->BasePath.move_to(ox + x, oy + y);
   }
   else if (Vector->wClose IS WVC_TOP) {
      Vector->BasePath.move_to(ox + width, oy); // Top right
      Vector->BasePath.line_to(ox, oy); // Top left
      Vector->BasePath.line_to(ox + x, oy + y);
   }
   else if (Vector->wClose IS WVC_BOTTOM) {
      Vector->BasePath.move_to(ox + width, oy + height); // Bottom right
      Vector->BasePath.line_to(ox, oy + height); // Bottom left
      Vector->BasePath.line_to(ox + x, oy + y);
   }
   else return;

   // Sine wave generator.  This applies scaling so that the correct number of vertices are generated.  Also, the
   // last vertex is interpolated to end exactly at 360, ensuring that the path terminates accurately.

   DOUBLE degree = Vector->wDegree;
   DOUBLE xscale = width * (1.0 / 360.0);
   DOUBLE freq = Vector->wFrequency * scale;
   DOUBLE angle;
   DOUBLE last_x = x, last_y = y;
   if (Vector->wDecay IS 1.0) {
      for (angle=scale; angle < 360; angle += scale, degree += freq) {
         DOUBLE x = angle * xscale;
         DOUBLE y = (sin(DEG2RAD * degree) * amp) + (height * 0.5);
         if (Vector->Transition) apply_transition_xy(Vector->Transition, angle * (1.0 / 360.0), &x, &y);
         if ((std::abs(x - last_x) >= 0.5) or (std::abs(y - last_y) >= 0.5)) {
            Vector->BasePath.line_to(ox + x, oy + y);
            last_x = x;
            last_y = y;
         }
      }
      degree -= freq;
      degree += freq * (360.0 - (angle - scale)) / scale;
      DOUBLE x = width;
      DOUBLE y = (sin(DEG2RAD * degree) * amp) + (height * 0.5);
      if (Vector->Transition) apply_transition_xy(Vector->Transition, angle * (1.0 / 360.0), &x, &y);
      Vector->BasePath.line_to(ox + x, oy + y);
   }
   else if (Vector->wDecay > 0) {
      for (angle=scale; angle < 360; angle += scale, degree += freq) {
         DOUBLE x = angle * xscale;
         DOUBLE y = (sin(DEG2RAD * degree) * amp) / exp((DOUBLE)angle / decay) + (height * 0.5);
         if ((std::abs(x - last_x) >= 0.5) or (std::abs(y - last_y) >= 0.5)) {
            Vector->BasePath.line_to(ox + x, oy + y);
            last_x = x;
            last_y = y;
         }
      }
      degree -= freq;
      degree += freq * (360.0 - (angle - scale)) / scale;
      DOUBLE x = width;
      DOUBLE y = (sin(DEG2RAD * degree) * amp) / exp(360.0 / decay) + (height * 0.5);
      if (Vector->Transition) apply_transition_xy(Vector->Transition, angle * (1.0 / 360.0), &x, &y);
      Vector->BasePath.line_to(ox + x, oy + y);
   }
   else if (Vector->wDecay < 0) {
      for (angle=scale; angle < 360; angle += scale, degree += freq) {
         DOUBLE x = angle * xscale;
         DOUBLE y = (sin(DEG2RAD * degree) * amp) / log((DOUBLE)angle / decay) + (height * 0.5);
         if (Vector->Transition) apply_transition_xy(Vector->Transition, angle * (1.0 / 360.0), &x, &y);
         if ((std::abs(x - last_x) >= 0.5) or (std::abs(y - last_y) >= 0.5)) {
            Vector->BasePath.line_to(ox + x, oy + y);
            last_x = x;
            last_y = y;
         }
      }
      degree -= freq;
      degree += freq * (360.0 - (angle - scale)) / scale;
      DOUBLE x = width;
      DOUBLE y = (sin(DEG2RAD * degree) * amp) / log(360.0 / decay) + (height * 0.5);
      if (Vector->Transition) apply_transition_xy(Vector->Transition, angle * (1.0 / 360.0), &x, &y);
      Vector->BasePath.line_to(ox + x, oy + y);
   }

   if (Vector->wThickness > 0) {
      DOUBLE x, y;
      LONG total = Vector->BasePath.total_vertices();
      Vector->BasePath.last_vertex(&x, &y);
      Vector->BasePath.line_to(x, y + Vector->wThickness);
      for (LONG i=total-1; i >= 0; i--) {
         Vector->BasePath.vertex(i, &x, &y);
         Vector->BasePath.line_to(x, y + Vector->wThickness);
      }
      Vector->BasePath.translate(0, -Vector->wThickness * 0.5); // Ensure that the wave is centered vertically.
   }

   if ((Vector->wClose) or (Vector->wThickness > 0)) Vector->BasePath.close_polygon();

   bounding_rect_single(Vector->BasePath, 0, &Vector->BX1, &Vector->BY1, &Vector->BX2, &Vector->BY2);
}

/*********************************************************************************************************************
-ACTION-
Move: Moves the vector to a new position.
-END-
*****************************************************************************/

static ERROR WAVE_Move(extVectorWave *Self, struct acMove *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   Self->wX += Args->DeltaX;
   Self->wY += Args->DeltaY;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
MoveToPoint: Moves the vector to a new fixed position.
-END-
*****************************************************************************/

static ERROR WAVE_MoveToPoint(extVectorWave *Self, struct acMoveToPoint *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->Flags & MTF_X) Self->wX = Args->X;
   if (Args->Flags & MTF_Y) Self->wY = Args->Y;
   if (Args->Flags & MTF_RELATIVE) Self->wDimensions = (Self->wDimensions | DMF_RELATIVE_X | DMF_RELATIVE_Y) & ~(DMF_FIXED_X | DMF_FIXED_Y);
   else Self->wDimensions = (Self->wDimensions | DMF_FIXED_X | DMF_FIXED_Y) & ~(DMF_RELATIVE_X | DMF_RELATIVE_Y);
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR WAVE_NewObject(extVectorWave *Self, APTR Void)
{
   Self->GeneratePath = (void (*)(extVector *))&generate_wave;
   Self->wFrequency = 1.0;
   Self->wAmplitude = 1.0;
   Self->wDecay = 1.0;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Resize: Changes the vector's area.
-END-
*****************************************************************************/

static ERROR WAVE_Resize(extVectorWave *Self, struct acResize *Args)
{
   if (!Args) return ERR_NullArgs;

   Self->wWidth = Args->Width;
   Self->wHeight = Args->Height;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Amplitude: Adjusts the generated wave amplitude.

The Amplitude is expressed as a multiplier that adjusts the wave amplitude (i.e. height).  A value of 1.0 is the
default.

*****************************************************************************/

static ERROR WAVE_GET_Amplitude(extVectorWave *Self, DOUBLE *Value)
{
   *Value = Self->wAmplitude;
   return ERR_Okay;
}

static ERROR WAVE_SET_Amplitude(extVectorWave *Self, DOUBLE Value)
{
   if (Value > 0.0) {
      Self->wAmplitude = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Close: Closes the generated wave path at either the top or bottom.

Setting the Close field to TOP or BOTTOM will close the generated wave's path so that it is suitable for being
filled.

*****************************************************************************/

static ERROR WAVE_GET_Close(extVectorWave *Self, LONG *Value)
{
   *Value = Self->wClose;
   return ERR_Okay;
}

static ERROR WAVE_SET_Close(extVectorWave *Self, LONG Value)
{
   Self->wClose = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Decay: Declares a rate of decay to apply to the wave amplitude.

The amplitude of a sine wave can be decayed between its start and end points by setting the Decay field.  Using a decay
gives the wave an appearance of being funnelled into a cone-like shape.  If the value is negative, the start and
end points for the decay will be reversed.

*****************************************************************************/

static ERROR WAVE_GET_Decay(extVectorWave *Self, DOUBLE *Value)
{
   *Value = Self->wDecay;
   return ERR_Okay;
}

static ERROR WAVE_SET_Decay(extVectorWave *Self, DOUBLE Value)
{
   Self->wDecay = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Degree: Declares the initial angle (in degrees) to use when generating the wave.

The degree value defines the initial angle that is used when computing the sine wave.  The default is zero.

Visually, changing the degree will affect the 'offset' of the generated wave.  Gradually incrementing the value
will give the wave an appearance of moving from right to left.

*****************************************************************************/

static ERROR WAVE_GET_Degree(extVectorWave *Self, DOUBLE *Value)
{
   *Value = Self->wDegree;
   return ERR_Okay;
}

static ERROR WAVE_SET_Degree(extVectorWave *Self, DOUBLE Value)
{
   Self->wDegree = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_HEIGHT">The #Height value is a fixed coordinate.</>
<type name="FIXED_WIDTH">The #Width value is a fixed coordinate.</>
<type name="FIXED_X">The #X value is a fixed coordinate.</>
<type name="FIXED_Y">The #Y value is a fixed coordinate.</>
<type name="RELATIVE_HEIGHT">The #Height value is a relative coordinate.</>
<type name="RELATIVE_WIDTH">The #Width value is a relative coordinate.</>
<type name="RELATIVE_X">The #X value is a relative coordinate.</>
<type name="RELATIVE_Y">The #Y value is a relative coordinate.</>
</types>

*****************************************************************************/

static ERROR WAVE_GET_Dimensions(extVectorWave *Self, LONG *Value)
{
   *Value = Self->wDimensions;
   return ERR_Okay;
}

static ERROR WAVE_SET_Dimensions(extVectorWave *Self, LONG Value)
{
   Self->wDimensions = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Frequency: Defines the wave frequency (the distance between each wave).

The frequency determines the distance between each individual wave that is generated.  The default
value for the frequency is 1.0.  Shortening the frequency to a value closer to 0 will bring the waves closer together.

*****************************************************************************/

static ERROR WAVE_GET_Frequency(extVectorWave *Self, DOUBLE *Value)
{
   *Value = Self->wFrequency;
   return ERR_Okay;
}

static ERROR WAVE_SET_Frequency(extVectorWave *Self, DOUBLE Value)
{
   if (Value > 0.0) {
      Self->wFrequency = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Height: The height of the area containing the wave.

The height of the area containing the wave is defined here as a fixed or relative value.

*****************************************************************************/

static ERROR WAVE_GET_Height(extVectorWave *Self, Variable *Value)
{
   DOUBLE val = Self->wHeight;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR WAVE_SET_Height(extVectorWave *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->wDimensions = (Self->wDimensions | DMF_RELATIVE_HEIGHT) & (~DMF_FIXED_HEIGHT);
   }
   else Self->wDimensions = (Self->wDimensions | DMF_FIXED_HEIGHT) & (~DMF_RELATIVE_HEIGHT);

   Self->wHeight = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Style: Selects an alternative wave style.

NOT YET IMPLEMENTED

By default, waves are generated in the style of a sine wave.  Alternative styles can be selected by setting this field.

*****************************************************************************/

static ERROR WAVE_GET_Style(extVectorWave *Self, LONG *Value)
{
   *Value = Self->wStyle;
   return ERR_Okay;
}

static ERROR WAVE_SET_Style(extVectorWave *Self, LONG Value)
{
   Self->wStyle = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Thickness: Expands the height of the wave to the specified value to produce a closed path.

Specifying a thickness value will create a wave that forms a filled shape, rather than the default of a stroked path.
The thickness (height) of the wave is determined by the provided value.

*****************************************************************************/

static ERROR WAVE_GET_Thickness(extVectorWave *Self, DOUBLE *Value)
{
   *Value = Self->wThickness;
   return ERR_Okay;
}

static ERROR WAVE_SET_Thickness(extVectorWave *Self, DOUBLE Value)
{
   Self->wThickness = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Width: The width of the area containing the wave.

The width of the area containing the wave is defined here as a fixed or relative value.

*****************************************************************************/

static ERROR WAVE_GET_Width(extVectorWave *Self, Variable *Value)
{
   DOUBLE val = Self->wWidth;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR WAVE_SET_Width(extVectorWave *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->wDimensions = (Self->wDimensions | DMF_RELATIVE_WIDTH) & (~DMF_FIXED_WIDTH);
   }
   else Self->wDimensions = (Self->wDimensions | DMF_FIXED_WIDTH) & (~DMF_RELATIVE_WIDTH);

   Self->wWidth = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
X: The x coordinate of the wave.  Can be expressed as a fixed or relative coordinate.

The x coordinate of the wave is defined here as either a fixed or relative value.

*****************************************************************************/

static ERROR WAVE_GET_X(extVectorWave *Self, Variable *Value)
{
   DOUBLE val = Self->wX;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR WAVE_SET_X(extVectorWave *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->wDimensions = (Self->wDimensions | DMF_RELATIVE_X) & (~DMF_FIXED_X);
   }
   else Self->wDimensions = (Self->wDimensions | DMF_FIXED_X) & (~DMF_RELATIVE_X);

   Self->wX = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Y: The y coordinate of the wave.  Can be expressed as a fixed or relative coordinate.

The y coordinate of the wave is defined here as either a fixed or relative value.
-END-
*****************************************************************************/

static ERROR WAVE_GET_Y(extVectorWave *Self, Variable *Value)
{
   DOUBLE val = Self->wY;
   if (Value->Type & FD_PERCENTAGE) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR WAVE_SET_Y(extVectorWave *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->wDimensions = (Self->wDimensions | DMF_RELATIVE_Y) & (~DMF_FIXED_Y);
   }
   else Self->wDimensions = (Self->wDimensions | DMF_FIXED_Y) & (~DMF_RELATIVE_Y);

   Self->wY = val;
   reset_path(Self);
   return ERR_Okay;
}

//********************************************************************************************************************

static const FieldDef clWaveClose[] = {
   { "None",   WVC_NONE },
   { "Top",    WVC_TOP },
   { "Bottom", WVC_BOTTOM },
   { NULL, 0 }
};

static const FieldDef clWaveStyle[] = {
   { "Curved",   WVS_CURVED },
   { "Angled",   WVS_ANGLED },
   { "Sawtooth", WVS_SAWTOOTH },
   { NULL, 0 }
};

static const FieldDef clWaveDimensions[] = {
   { "FixedHeight",     DMF_FIXED_HEIGHT },
   { "FixedWidth",      DMF_FIXED_WIDTH },
   { "FixedX",          DMF_FIXED_X },
   { "FixedY",          DMF_FIXED_Y },
   { "RelativeHeight",  DMF_RELATIVE_HEIGHT },
   { "RelativeWidth",   DMF_RELATIVE_WIDTH },
   { "RelativeX",       DMF_RELATIVE_X },
   { "RelativeY",       DMF_RELATIVE_Y },
   { NULL, 0 }
};

static const FieldArray clWaveFields[] = {
   { "Amplitude",  FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,    0, (APTR)WAVE_GET_Amplitude, (APTR)WAVE_SET_Amplitude },
   { "Close",      FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clWaveClose, (APTR)WAVE_GET_Close, (APTR)WAVE_SET_Close },
   { "Decay",      FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,    0, (APTR)WAVE_GET_Decay, (APTR)WAVE_SET_Decay },
   { "Degree",     FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,    0, (APTR)WAVE_GET_Degree, (APTR)WAVE_SET_Degree },
   { "Dimensions", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, (MAXINT)&clWaveDimensions, (APTR)WAVE_GET_Dimensions, (APTR)WAVE_SET_Dimensions },
   { "Frequency",  FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,    0, (APTR)WAVE_GET_Frequency, (APTR)WAVE_SET_Frequency },
   { "Height",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)WAVE_GET_Height, (APTR)WAVE_SET_Height },
   { "Style",      FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clWaveStyle, (APTR)WAVE_GET_Style, (APTR)WAVE_SET_Style },
   { "Thickness",  FDF_VIRTUAL|FDF_DOUBLE|FDF_RW,    0, (APTR)WAVE_GET_Thickness, (APTR)WAVE_SET_Thickness },
   { "X",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)WAVE_GET_X, (APTR)WAVE_SET_X },
   { "Y",          FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)WAVE_GET_Y, (APTR)WAVE_SET_Y },
   { "Width",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)WAVE_GET_Width, (APTR)WAVE_SET_Width },
   END_FIELD
};

static const ActionArray clWaveActions[] = {
   { AC_NewObject,     (APTR)WAVE_NewObject },
   { AC_Move,          (APTR)WAVE_Move },
   { AC_MoveToPoint,   (APTR)WAVE_MoveToPoint },
   //{ AC_Redimension, (APTR)WAVE_Redimension },
   { AC_Resize,      (APTR)WAVE_Resize },
   { 0, NULL }
};

static ERROR init_wave(void)
{
   clVectorWave = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::SubClassID(ID_VECTORWAVE),
      fl::Name("VectorWave"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clWaveActions),
      fl::Fields(clWaveFields),
      fl::Size(sizeof(extVectorWave)),
      fl::Path(MOD_PATH));

   return clVectorWave ? ERR_Okay : ERR_AddClass;
}
