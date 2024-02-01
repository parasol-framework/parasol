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

static void reset_bounds(extVectorClip *Self)
{
   Self->BX1 = FLT_MAX;
   Self->BY1 = FLT_MAX;
   Self->BX2 = -FLT_MAX;
   Self->BY2 = -FLT_MAX;
}

//********************************************************************************************************************
// Basic function for recursively drawing all child vectors to a bitmap mask.

static void draw_clips(extVectorClip *Self, extVector *Branch,
   agg::rasterizer_scanline_aa<> &Rasterizer,
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> &Solid)
{
   agg::scanline32_p8 sl;
   for (auto scan=Branch; scan; scan=(extVector *)scan->Next) {
      if (scan->Class->BaseClassID IS ID_VECTOR) {
         agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(scan->BasePath, scan->Transform);
         Rasterizer.reset();
         Rasterizer.add_path(final_path);
         agg::render_scanlines(Rasterizer, sl, Solid);
      }

      if (scan->Child) draw_clips(Self, (extVector *)scan->Child, Rasterizer, Solid);
   }
}

//********************************************************************************************************************
// Called by the scene graph renderer

extern void draw_clipmask(extVectorClip *Self, extVector *ClientShape)
{
   pf::Log log;

   // Ensure that the Bounds are up to date and refresh them if necessary.

   if ((Self->Child) and (!Self->RefreshBounds)) {
      if (check_branch_dirty((extVector *)Self->Child)) Self->RefreshBounds = true;
   }

   if (Self->RefreshBounds) {
      // Calculate the bounds of all the paths defined and contained by the clip object

      Self->RefreshBounds = false;

      reset_bounds(Self);

      if (!Self->BasePath.empty()) { // Get the boundary of the ClipPath if defined by the client.
         bounding_rect_single(Self->BasePath, 0, &Self->BX1, &Self->BY1, &Self->BX2, &Self->BY2);
      }

      if (Self->Child) {
         std::array<DOUBLE, 4> bounds = { Self->BX1, Self->BY1, Self->BX2, Self->BY2 };
         calc_full_boundary((extVector *)Self->Child, bounds);
         Self->BX1 = bounds[0];
         Self->BY1 = bounds[1];
         Self->BX2 = bounds[2];
         Self->BY2 = bounds[3];
      }
   }
   else {
      // If the bounds don't require refreshing then the existing ClipData is also going to be current,
      // so we can return immediately.
      return;
   }

   if (Self->BX1 IS FLT_MAX) return; // Return if no paths were defined.

   // Allocate a bitmap that is large enough to contain the mask (persists between draw sessions).

   LONG width  = F2T(Self->BX2) + 1;
   LONG height = F2T(Self->BY2) + 1;

   if ((width <= 0) or (height <= 0)) {
      DEBUG_BREAK
      log.warning(ERR_InvalidDimension);
      return;
   }

   if ((width > 4096) or (height > 4096)) {
      log.warning("Mask size of %dx%d pixels exceeds imposed limits.", width, height);
      if (width > 4096)  width = 4096;
      if (height > 4096) height = 4096;
   }

   #ifdef DBG_DRAW
      log.trace("Drawing clipping mask with bounds %g %g %g %g (%dx%d)", Self->Bounds[0], Self->Bounds[1], Self->Bounds[2], Self->Bounds[3], width, height);
   #endif

   size_t size = width * height;
   if (Self->ClipData.size() < size) Self->ClipData.resize(size);

   // Configure an 8-bit monochrome bitmap for holding the mask

   Self->ClipRenderer.attach(Self->ClipData.data(), width-1, height-1, width);
   agg::pixfmt_gray8 pixf(Self->ClipRenderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> solid(rb);
   agg::rasterizer_scanline_aa<> rasterizer;

   ClearMemory(Self->ClipData.data(), Self->ClipData.size());

   solid.color(agg::gray8(0xff, 0xff));

   // Every child vector of the VectorClip that exports a path will be rendered to the mask.

   if (Self->Child) draw_clips(Self, (extVector *)Self->Child, rasterizer, solid);

   // A client can provide its own clipping path by setting ClipPath property.  This is more optimal than
   // using child vectors - the VectorViewport is one such client that uses this feature.

   if (!Self->BasePath.empty()) {
      agg::scanline32_p8 sl;
      agg::path_storage final_path(Self->BasePath);

      rasterizer.reset();
      rasterizer.add_path(final_path);
      agg::render_scanlines(rasterizer, sl, solid);
   }
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

   Self->ClipUnits  = VUNIT::BOUNDING_BOX;
   Self->Visibility = VIS::HIDDEN; // Because the content of the clip object must be ignored by the core vector drawing routine.
   Self->RefreshBounds = true;
   reset_bounds(Self);
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

