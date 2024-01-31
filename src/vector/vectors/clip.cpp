/*********************************************************************************************************************

-CLASS-
VectorClip: Clips are used to define complex clipping regions for vectors.

The VectorClip defines a clipping path that can be used by other vectors as a mask.  The clipping path is defined by
creating Vector shapes that are initialised to the VectorClip as child objects.

Any Vector that defines a path can utilise a VectorClip by referencing it through the Vector's Mask field.

VectorClip objects must always be owned by their relevant @VectorScene or @VectorViewport.  It is valid for a VectorClip
to be shared by multiple vector objects within the same scene.

-END-

*********************************************************************************************************************/

//********************************************************************************************************************
// NB: Considered a shape (can be transformed)

static void draw_clips(extVectorClip *Self, extVector *Branch,
   agg::rasterizer_scanline_aa<> &Rasterizer,
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> &Solid)
{
   agg::scanline_p8 sl;
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

/*********************************************************************************************************************
-ACTION-
Name:  Draw
Short: Renders the vector clipping shape(s) to an internal buffer.
-END-
*********************************************************************************************************************/

static ERROR CLIP_Draw(extVectorClip *Self, struct acDraw *Args)
{
   pf::Log log;

   // Calculate the bounds of all the paths defined and contained by the clip object

   std::array<DOUBLE, 4> bounds = { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX };

   if (Self->ClipPath) {
      // The ClipPath is internal and can be used by the likes of VectorViewport.
      bounding_rect_single(*Self->ClipPath, 0, &bounds[0], &bounds[1], &bounds[2], &bounds[3]);
   }

   if (Self->Child) calc_full_boundary((extVector *)Self->Child, bounds);

   if (bounds[0] >= 1000000) return ERR_Okay; // Return if there are no valid paths.

   LONG width = bounds[2] + 1; // Vector->BX2 - Vector->BX1 + 1;
   LONG height = bounds[3] + 1; // Vector->BY2 - Vector->BY1 + 1;

   if ((width <= 0) or (height <= 0)) {
      log.warning("Invalid mask size of %dx%d detected.", width, height);
      DEBUG_BREAK
   }

   if (width < 0) width = -width;
   else if (!width) width = 1;

   if (height < 0) height = -height;
   else if (!height) height = 1;

   if ((width > 4096) or (height > 4096)) {
      log.warning("Mask size of %dx%d pixels exceeds imposed limits.", width, height);
      if (width > 4096)  width = 4096;
      if (height > 4096) height = 4096;
   }

   #ifdef DBG_DRAW
      log.trace("Drawing clipping mask with bounds %.2f %.2f %.2f %.2f (%dx%d)", bounds[0], bounds[1], bounds[2], bounds[3], width, height);
   #endif

   LONG size = width * height;
   if ((Self->ClipData) and (size > Self->ClipSize)) {
      FreeResource(Self->ClipData);
      Self->ClipData = NULL;
      Self->ClipSize = 0;
   }

   if (!Self->ClipData) {
      if (!AllocMemory(size, MEM::DATA|MEM::NO_CLEAR, &Self->ClipData)) {
         Self->ClipSize = size;
      }
      else return ERR_AllocMemory;
   }

   Self->ClipRenderer.attach(Self->ClipData, width-1, height-1, width);
   agg::pixfmt_gray8 pixf(Self->ClipRenderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> solid(rb);
   agg::rasterizer_scanline_aa<> rasterizer;

   ClearMemory(Self->ClipData, Self->ClipSize);

   solid.color(agg::gray8(0xff, 0xff));

   // Every child vector of the VectorClip that exports a path will be rendered to the mask.

   if (Self->Child) draw_clips(Self, (extVector *)Self->Child, rasterizer, solid);

   // Internal paths can only be set by other vector classes, such as VectorViewport.

   if (Self->ClipPath) {
      agg::rasterizer_scanline_aa<> rasterizer;
      agg::scanline_p8 sl;
      agg::path_storage final_path(*Self->ClipPath);
      rasterizer.reset();
      rasterizer.add_path(final_path);
      agg::render_scanlines(rasterizer, sl, solid);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIP_Free(extVectorClip *Self, APTR Void)
{
   if (Self->ClipData) { FreeResource(Self->ClipData); Self->ClipData = NULL; }
   if (Self->ClipPath) { delete Self->ClipPath; Self->ClipPath = NULL; }

   using agg::rendering_buffer;
   Self->ClipRenderer.~rendering_buffer();
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
   Self->ClipUnits  = VUNIT::BOUNDING_BOX;
   Self->Visibility = VIS::HIDDEN; // Because the content of the clip object must be ignored by the core vector drawing routine.
   new (&Self->ClipRenderer) agg::rendering_buffer;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Transform: Applies a transform to the paths in the clipping mask.

A transform can be applied to the paths in the clipping mask by setting this field with an SVG compliant transform
string.

*********************************************************************************************************************/

static ERROR CLIP_SET_Transform(extVectorClip *Self, CSTRING Commands)
{
   pf::Log log;

   if (!Commands) return log.warning(ERR_InvalidValue);

   if (!Self->Matrices) {
      VectorMatrix *matrix;
      if (!vecNewMatrix(Self, &matrix)) return vecParseTransform(matrix, Commands);
      else return ERR_CreateResource;
   }
   else {
      vecResetMatrix(Self->Matrices);
      return vecParseTransform(Self->Matrices, Commands);
   }
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
   Self->ClipUnits = Value;
   return ERR_Okay;
}

//********************************************************************************************************************

#include "clip_def.cpp"

static const ActionArray clClipActions[] = {
   { AC_Draw,      CLIP_Draw },
   { AC_Free,      CLIP_Free },
   { AC_Init,      CLIP_Init },
   { AC_NewObject, CLIP_NewObject },
   { 0, NULL }
};

static const FieldArray clClipFields[] = {
   { "Units",     FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, CLIP_GET_Units, CLIP_SET_Units, &clVectorClipVUNIT },
   { "Transform", FDF_VIRTUAL|FDF_STRING|FDF_W, NULL, CLIP_SET_Transform },
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

