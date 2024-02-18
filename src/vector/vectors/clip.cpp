/*********************************************************************************************************************

-CLASS-
VectorClip: Clips are used to define complex clipping regions for vectors.

The VectorClip defines a clipping path that can be used by other vectors as a mask.  The clipping path is defined by
creating Vector shapes that are initialised to the VectorClip as child objects.

Any Vector that defines a path can utilise a VectorClip by referencing it through the Vector's Mask field.

VectorClip objects must be owned by a @VectorScene.  It is valid for a VectorClip to be shared by multiple vector 
objects within the same scene.  If optimum drawing efficiency is required, we recommend that each VectorClip is 
referenced by one vector only.  This will reduce the frequency of path recomputation and redrawing of the clipping 
path.

-END-

*********************************************************************************************************************/

// Path generation for the VectorClip.  Since the requisite paths already exist in the Viewport child objects,
// the only job we need to do here is to compute the boundary.  Regular paths are additive to the overall clipping
// shape, whilst viewports confined by overflow settings are restrictive (with respect to their content).

void generate_clip(extVectorClip *Clip)
{
   std::function<void(extVector *, bool)> scan_bounds;
   TClipRectangle<DOUBLE> b = TCR_EXPANDING;
   DOUBLE largest_stroke = 0;

   scan_bounds = [&b, &scan_bounds, &largest_stroke](extVector *Branch, bool IncSiblings) -> void {
      for (auto node=Branch; node; node=(extVector *)node->Next) {
         if (node->dirty()) gen_vector_path(node);

         if (node->Class->ClassID IS ID_VECTORVIEWPORT) {
            // For the sake of keeping things simple, viewports are treated as containers but this is not optimal if
            // the overflow settings restrict content.  Any optimisation would require tests to be constructed first.
         }
         else if (node->Class->BaseClassID IS ID_VECTOR) {
            if (node->Transform.is_normal()) b.expanding(node);
            else {
               auto path = node->Bounds.as_path(node->Transform);
               b.expanding(get_bounds(path));
            }

            if (node->Stroked) {
               auto sw = node->fixed_stroke_width() * node->Transform.scale();
               if (sw > largest_stroke) largest_stroke = sw;
            }
         }

         if (node->Child) scan_bounds((extVector *)node->Child, true);
      }
   };

   // The scan starts from our hosting viewport to ensure that its path information is generated.

   if (Clip->Viewport) scan_bounds((extVector *)Clip->Viewport, true);

   Clip->LargestStroke = largest_stroke;
   Clip->Bounds = b;
}

//********************************************************************************************************************

static ERROR CLIP_Free(extVectorClip *Self, APTR Void)
{
   Self->~extVectorClip();
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIP_Init(extVectorClip *Self, APTR Void)
{
   pf::Log log;

   if ((LONG(Self->ClipUnits) <= 0) or (LONG(Self->ClipUnits) >= LONG(VUNIT::END))) {
      log.traceWarning("Invalid Units value of %d", Self->ClipUnits);
      return ERR_OutOfRange;
   }

   if (!Self->Parent) return ERR_FieldNotSet;
   else if (Self->Parent->Class->ClassID IS ID_VECTORSCENE) {
      // A 'dummy' viewport hosts the shapes for determining the clipping path.  This allows us to
      // control the boundary size according to the Units value.

      Self->Viewport = (extVectorViewport *)objVectorViewport::create::global(
         fl::Visibility(VIS::HIDDEN),
         fl::AspectRatio(ARF::NONE),
         fl::X(0), fl::Y(0), fl::Width(1), fl::Height(1) // Target dimensions are defined when drawing
      );

      if (Self->ClipUnits IS VUNIT::BOUNDING_BOX) {
         // In BOUNDING_BOX mode the clip paths will be sized within a viewbox of (0 0 1 1) as required by SVG
         Self->Viewport->setFields(fl::ViewWidth(1.0), fl::ViewHeight(1.0));
      }
   }
   else return log.warning(ERR_UnsupportedOwner);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIP_NewChild(extVectorClip *Self, struct acNewChild *Args)
{
   if (Self->initialised()) {
      pf::Log log;
      log.warning("Child objects not supported - assign this %s to Viewport instead.", Args->Object->className());
      return ERR_NoSupport;
   }
   else return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIP_NewObject(extVectorClip *Self, APTR Void)
{
   new (Self) extVectorClip;

   Self->GeneratePath  = (void (*)(extVector *))&generate_clip;
   Self->ClipUnits     = VUNIT::USERSPACE; // SVG default is userSpaceOnUse
   Self->Visibility    = VIS::HIDDEN; // Because the content of the clip object must be ignored by the core vector drawing routine.
   Self->RefreshBounds = true;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
ClipFlags: Optional flags.
Lookup: VCLF

-END-
*********************************************************************************************************************/

static ERROR CLIP_GET_ClipFlags(extVectorClip *Self, VCLF *Value)
{
   *Value = Self->ClipFlags;
   return ERR_Okay;
}

static ERROR CLIP_SET_ClipFlags(extVectorClip *Self, VCLF Value)
{
   Self->ClipFlags = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Units: Defines the coordinate system for fields X, Y, Width and Height.

The default coordinate system for clip-paths is `BOUNDING_BOX`, which positions the clipping region relative to the 
vector that references it.  The alternative is `USERSPACE`, which positions the path relative to the vector's parent 
viewport.
-END-
*********************************************************************************************************************/

static ERROR CLIP_GET_Units(extVectorClip *Self, VUNIT *Value)
{
   *Value = Self->ClipUnits;
   return ERR_Okay;
}

static ERROR CLIP_SET_Units(extVectorClip *Self, VUNIT Value)
{
   Self->RefreshBounds = true;
   Self->ClipUnits = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Viewport: This viewport hosts the Vector objects that will contribute to the clip path.

To define the path(s) that will be used to build the clipping mask, add at least one @Vector object to the viewport 
declared here.
-END-
*********************************************************************************************************************/

static ERROR CLIP_GET_Viewport(extVectorClip *Self, extVectorViewport **Value)
{
   *Value = Self->Viewport;
   return ERR_Okay;
}

//********************************************************************************************************************

#include "clip_def.cpp"

static const ActionArray clClipActions[] = {
   { AC_Free,      CLIP_Free },
   { AC_Init,      CLIP_Init },
   { AC_NewChild,  CLIP_NewChild },
   { AC_NewObject, CLIP_NewObject },
   { 0, NULL }
};

static const FieldArray clClipFields[] = {
   { "Units", FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, CLIP_GET_Units, CLIP_SET_Units, &clVectorClipVUNIT },
   { "Viewport", FDF_VIRTUAL|FDF_OBJECT|FDF_R, CLIP_GET_Viewport },
   { "ClipFlags", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, CLIP_GET_ClipFlags, CLIP_SET_ClipFlags, &clVectorClipVCLF },
   END_FIELD
};

static ERROR init_clip(void)
{
   clVectorClip = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::ClassID(ID_VECTORCLIP),
      fl::Name("VectorClip"),
      fl::Actions(clClipActions),
      fl::Fields(clClipFields),
      fl::Category(CCF::GRAPHICS),
      fl::Size(sizeof(extVectorClip)),
      fl::Path(MOD_PATH));

   return clVectorClip ? ERR_Okay : ERR_AddClass;
}

