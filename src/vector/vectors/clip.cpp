/*********************************************************************************************************************

-CLASS-
VectorClip: Clips are used to define complex clipping regions for vectors.

The VectorClip defines a clipping path that can be used by other vectors as a mask.  The clipping path is defined by
creating Vector shapes that are initialised to the VectorClip as child objects.

Any Vector that defines a path can utilise a VectorClip by referencing it through the Vector's Mask field.

VectorClip objects must be owned by their relevant @VectorScene or @VectorViewport.  It is valid for a VectorClip
to be shared by multiple vector objects within the same scene.  We recommend that for optimum drawing efficiency, any
given VectorClip is associated with one vector only.  This will reduce the chances of a redraw being required at
any given time.

-END-

*********************************************************************************************************************/

// Path generation for the VectorClip.  Since the requisite paths already exist in child objects and the BasePath,
// the only job we need to do here is to compute the boundary.  Regular paths are additive to the overall clipping
// shape, whilst viewports are restrictive.

static void generate_clip(extVectorClip *Clip)
{
   TClipRectangle<DOUBLE> b;

   DOUBLE largest_stroke = 0;

   std::function<void(extVector *, bool)> scan_bounds;

   scan_bounds = [&b, &scan_bounds, &largest_stroke](extVector *Branch, bool IncSiblings) -> void {
      for (auto scan=Branch; scan; scan=(extVector *)scan->Next) {
         if (scan->dirty()) gen_vector_path(scan);

         if (scan->Class->ClassID IS ID_VECTORVIEWPORT) {
            // If vpClipMask is set, then the viewport is masked.  Unmasked viewports can be ignored.
            if (((extVectorViewport *)scan)->vpClipMask) {
               auto vp_bounds = ((extVectorViewport *)scan)->vpClipMask;
               b.shrinking(vp_bounds);
            }
            if (((extVectorViewport *)scan)->ClipMask) {
               auto vp_bounds = ((extVectorViewport *)scan)->ClipMask;
               b.shrinking(vp_bounds);
            }
         }
         else if (scan->Class->BaseClassID IS ID_VECTOR) {           
            TClipRectangle bounds;
            if (scan->Transform.is_normal()) {
               b.expanding(scan);
            }
            else {
               auto path = scan->Bounds.as_path(scan->Transform);
               b.expanding(get_bounds(path));
            }

            if (scan->Stroked) {
               if (scan->StrokeWidth > largest_stroke) largest_stroke = scan->StrokeWidth;
            }
         }

         if (scan->Child) scan_bounds((extVector *)scan->Child, true);
      }
   };

   b = TCR_EXPANDING;

   if (Clip->Child) scan_bounds((extVector *)Clip->Child, true);
   
   if (!Clip->BasePath.empty()) {
      b.shrinking(get_bounds(Clip->BasePath));
   }

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

   if ((!Self->Parent) or ((Self->Parent->Class->ClassID != ID_VECTORSCENE) and (Self->Parent->Class->ClassID != ID_VECTORVIEWPORT))) {
      log.warning("This VectorClip object must be a child of a Scene or Viewport object.");
      return ERR_Failed;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIP_NewObject(extVectorClip *Self, APTR Void)
{
   new (Self) extVectorClip;

   Self->GeneratePath  = (void (*)(extVector *))&generate_clip;
   Self->ClipUnits     = VUNIT::BOUNDING_BOX;
   Self->Visibility    = VIS::HIDDEN; // Because the content of the clip object must be ignored by the core vector drawing routine.
   Self->RefreshBounds = true;
   Self->Viewport      = false;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Units: Defines the coordinate system for fields X, Y, Width and Height.

The default coordinate system for clip-paths is `BOUNDING_BOX`, which positions the clipping region against the vector
that references it.  The alternative is `USERSPACE`, which positions the path relative to the current viewport.
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

//********************************************************************************************************************

#include "clip_def.cpp"

static const ActionArray clClipActions[] = {
   { AC_Free,      CLIP_Free },
   { AC_Init,      CLIP_Init },
   { AC_NewObject, CLIP_NewObject },
   { 0, NULL }
};

static const FieldArray clClipFields[] = {
   { "Units", FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, CLIP_GET_Units, CLIP_SET_Units, &clVectorClipVUNIT },
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

