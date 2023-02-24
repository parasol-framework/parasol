/*********************************************************************************************************************

-CLASS-
VectorTransition: Transitions are used to gradually apply transforms over distance.

The VectorTransition class is used to gradually transform vector shapes over the length of a path.  This is a
special feature that is not SVG compliant, though it can be utilised from SVG files via the 'parasol:' name space.

The transition is defined as a series of stops and transform instructions, of which at least 2 are required in order to
interpolate the transforms over distance.  The transform strings are defined as per the SVG guidelines for the
transform attribute.

The following example illustrates the use of a transition in SVG:

<pre>
  &lt;defs&gt;
    &lt;parasol:transition id="hill"&gt;
      &lt;stop offset="0" transform="scale(0.3)"/&gt;
      &lt;stop offset="50%" transform="scale(1.5)"/&gt;
      &lt;stop offset="100%" transform="scale(0.3)"/&gt;
    &lt;/parasol:transition&gt;
  &lt;/defs&gt;

  &lt;rect fill="#ffffff" width="100%" height="100%"/&gt;
  &lt;text x="3" y="80" font-size="19.6" fill="navy" transition="url(#hill)"&gt;This text is morphed by a transition&lt;/text&gt;
</pre>

Transitions are most effective when used in conjunction with the morph feature in the @Vector class.

-END-

*********************************************************************************************************************/

// Applies the correct transform when given a relative Index position between 0.0 and 1.0

void apply_transition(objVectorTransition *Self, DOUBLE Index, agg::trans_affine &Transform)
{
   if (Index <= Self->Stops[0].Offset) {
      Transform.multiply(*Self->Stops[0].AGGTransform);
   }
   else if (Index >= Self->Stops[Self->TotalStops-1].Offset) {
      Transform.multiply(*Self->Stops[Self->TotalStops-1].AGGTransform);
   }
   else {
      // Interpolate between transforms.

      LONG left, right;
      for (left=Self->TotalStops-1; (left > 0) and (Index < Self->Stops[left].Offset); left--);
      for (right=left+1; (right < Self->TotalStops) and (Self->Stops[right].Offset < Index); right++);

      if ((left < right) and (right < Self->TotalStops)) {
         agg::trans_affine interp;

         // Normalise the index
         DOUBLE scale = (Index - Self->Stops[left].Offset) / (Self->Stops[right].Offset - Self->Stops[left].Offset);

         interp.sx  = Self->Stops[left].AGGTransform->sx  + ((Self->Stops[right].AGGTransform->sx  - Self->Stops[left].AGGTransform->sx) * scale);
         interp.sy  = Self->Stops[left].AGGTransform->sy  + ((Self->Stops[right].AGGTransform->sy  - Self->Stops[left].AGGTransform->sy) * scale);
         interp.shx = Self->Stops[left].AGGTransform->shx + ((Self->Stops[right].AGGTransform->shx - Self->Stops[left].AGGTransform->shx) * scale);
         interp.shy = Self->Stops[left].AGGTransform->shy + ((Self->Stops[right].AGGTransform->shy - Self->Stops[left].AGGTransform->shy) * scale);
         interp.tx  = Self->Stops[left].AGGTransform->tx  + ((Self->Stops[right].AGGTransform->tx  - Self->Stops[left].AGGTransform->tx) * scale);
         interp.ty  = Self->Stops[left].AGGTransform->ty  + ((Self->Stops[right].AGGTransform->ty  - Self->Stops[left].AGGTransform->ty) * scale);

         Transform.multiply(interp);

         //log.trace("Index: %.2f, Scale: %.2f, Left: %d, Right: %d, TotalStops: %d", Index, scale, left, right, Self->TotalStops);
      }
      else {
         pf::Log log(__FUNCTION__);
         log.warning("Invalid transition.  Index: %.2f, Left: %d, Right: %d, TotalStops: %d", Index, left, right, Self->TotalStops);
      }
   }
}

//********************************************************************************************************************
// Accurately interpolate the transform for Index and apply it to the coordinate (X,Y).

void apply_transition_xy(objVectorTransition *Self, DOUBLE Index, DOUBLE *X, DOUBLE *Y)
{
   if (Index <= Self->Stops[0].Offset) {
      Self->Stops[0].AGGTransform->transform(X, Y);
   }
   else if (Index >= Self->Stops[Self->TotalStops-1].Offset) {
      Self->Stops[Self->TotalStops-1].AGGTransform->transform(X, Y);
   }
   else {
      // Interpolate between transforms.

      LONG left, right;
      for (left=0; (left < Self->TotalStops) and (Index < Self->Stops[left].Offset); left++);
      for (right=left+1; (right < Self->TotalStops) and (Self->Stops[right].Offset < Index); right++);

      if ((left < right) and (right < Self->TotalStops)) {
         agg::trans_affine interp;

         // Normalise the index
         DOUBLE scale = (Index - Self->Stops[left].Offset) / (Self->Stops[right].Offset - Self->Stops[left].Offset);

         interp.sx  = Self->Stops[left].AGGTransform->sx  + ((Self->Stops[right].AGGTransform->sx  - Self->Stops[left].AGGTransform->sx) * scale);
         interp.sy  = Self->Stops[left].AGGTransform->sy  + ((Self->Stops[right].AGGTransform->sy  - Self->Stops[left].AGGTransform->sy) * scale);
         interp.shx = Self->Stops[left].AGGTransform->shx + ((Self->Stops[right].AGGTransform->shx - Self->Stops[left].AGGTransform->shx) * scale);
         interp.shy = Self->Stops[left].AGGTransform->shy + ((Self->Stops[right].AGGTransform->shy - Self->Stops[left].AGGTransform->shy) * scale);
         interp.tx  = Self->Stops[left].AGGTransform->tx  + ((Self->Stops[right].AGGTransform->tx  - Self->Stops[left].AGGTransform->tx) * scale);
         interp.ty  = Self->Stops[left].AGGTransform->ty  + ((Self->Stops[right].AGGTransform->ty  - Self->Stops[left].AGGTransform->ty) * scale);

         interp.transform(X, Y);
      }
   }
}

//********************************************************************************************************************

static ERROR set_stop_transform(objVectorTransition *Self, TransitionStop *Stop, CSTRING Commands)
{
   pf::Log log;

   log.traceBranch("%s", Commands);

   Self->Dirty = true;
   if (!Commands) Commands = ""; // Empty transforms are permitted - it will result in an identity matrix being created.

   vecParseTransform(&Stop->Matrix, Commands);

   auto &m = Stop->Matrix;
   if (Stop->AGGTransform) {
      Stop->AGGTransform->load_all(m.ScaleX, m.ShearY, m.ShearX, m.ScaleY, m.TranslateX, m.TranslateY);
      return ERR_Okay;
   }
   else {
      Stop->AGGTransform = new (std::nothrow) agg::trans_affine(m.ScaleX, m.ShearY, m.ShearX, m.ScaleY, m.TranslateX, m.TranslateY);
      if (Stop->AGGTransform) return ERR_Okay;
      else return log.warning(ERR_AllocMemory);
   }
}

//********************************************************************************************************************

static ERROR TRANSITION_Free(objVectorTransition *Self, APTR Void)
{
   for (auto i=0; i < Self->TotalStops; i++) {
      delete Self->Stops[i].AGGTransform;
   }
   Self->TotalStops = 0;

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR TRANSITION_Init(objVectorTransition *Self, APTR Void)
{
   pf::Log log;
   if (Self->TotalStops < 2) return log.warning(ERR_FieldNotSet);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR TRANSITION_NewObject(objVectorTransition *Self, APTR Void)
{
   Self->Dirty = true;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Stops: Defines the transforms that will be used at specific stop points.

A valid transition object must consist of at least two stop points in order to transition from one transform to another.
This is achieved by setting the Stops field with an array of Transition structures that define each stop point with
a transform string.  The Transition structure consists of the following fields:

<fields>
<field name="Offset" type="DOUBLE">An offset in the range of 0 to 1.0.</field>
<field name="Transform" type="STRING">A transform string, as per SVG guidelines.</field>
</fields>

*********************************************************************************************************************/

static ERROR TRANSITION_SET_Stops(objVectorTransition *Self, Transition *Value, LONG Elements)
{
   pf::Log log;
   if ((Elements >= 2) and (Elements < MAX_TRANSITION_STOPS)) {
      Self->TotalStops = Elements;
      DOUBLE last_offset = 0;
      for (auto i=0; i < Elements; i++) {
         if (Value[i].Offset < last_offset) return log.warning(ERR_InvalidValue); // Offsets must be in incrementing order.
         if ((Value[i].Offset < 0.0) or (Value[i].Offset > 1.0)) return log.warning(ERR_OutOfRange);

         Self->Stops[i].Offset = Value[i].Offset;
         set_stop_transform(Self, &Self->Stops[i], Value[i].Transform);

         last_offset = Value[i].Offset;
      }
      return ERR_Okay;
   }
   else return log.warning(ERR_DataSize);
}

/*********************************************************************************************************************

-FIELD-
TotalStops: Total number of stops defined in the Stops array.

This read-only field indicates the total number of stops that have been defined in the #Stops array.
-END-

*********************************************************************************************************************/

static const ActionArray clTransitionActions[] = {
   { AC_Free,      (APTR)TRANSITION_Free },
   { AC_Init,      (APTR)TRANSITION_Init },
   { AC_NewObject, (APTR)TRANSITION_NewObject },
   { 0, NULL }
};

static const FieldArray clTransitionFields[] = {
   { "TotalStops",   FDF_LONG|FDF_R, 0, NULL, NULL },
   // Virtual fields
   { "Stops",        FDF_VIRTUAL|FDF_ARRAY|FDF_STRUCT|FDF_W, (MAXINT)"Transition", NULL, (APTR)TRANSITION_SET_Stops },
   END_FIELD
};

ERROR init_transition(void) // The transition is a definition type for creating transitions and not drawing.
{
   clVectorTransition = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTORTRANSITION),
      fl::Name("VectorTransition"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clTransitionActions),
      fl::Fields(clTransitionFields),
      fl::Size(sizeof(objVectorTransition)),
      fl::Path(MOD_PATH));

   return clVectorTransition ? ERR_Okay : ERR_AddClass;
}

