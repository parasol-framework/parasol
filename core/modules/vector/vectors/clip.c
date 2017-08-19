/*****************************************************************************

-CLASS-
VectorClip: Clips are used to define complex clipping regions for vectors.

The VectorClip defines a clipping path that can be used by other vectors as a mask.  The clipping path is defined by
creating Vector shapes that are initialised to the VectorClip as child objects.

Any Vector that defines a shape can utilise a VectorClip by referencing it through the Vector's Mask field.

VectorClip objects must always be owned by their relevant @VectorScene or @VectorViewport.  It is valid for a VectorClip
to be shared by multiple vector objects within the same scene.

-END-

*****************************************************************************/

static void draw_clips(objVectorClip *Self, objVector *Branch,
   agg::rasterizer_scanline_aa<> &rasterizer,
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> &solid)
{
   agg::scanline_p8 sl;
   for (objVector *scan=Branch; scan; scan=(objVector *)scan->Next) {
      if (scan->Head.ClassID IS ID_VECTOR) {
         if (scan->BasePath) {
            agg::conv_transform<agg::path_storage, agg::trans_affine> final_path(*scan->BasePath, *scan->Transform);
            rasterizer.reset();
            rasterizer.add_path(final_path);
            agg::render_scanlines(rasterizer, sl, solid);
         }
      }

      if (scan->Child) draw_clips(Self, (objVector *)scan->Child, rasterizer, solid);
   }
}

//****************************************************************************

static ERROR CLIP_Draw(objVectorClip *Self, struct acDraw *Args)
{
   // Calculate the bounds of all the paths defined and contained by the clip object

   std::array<DOUBLE, 4> bounds = { 1000000, 1000000, -1000000, -1000000 };

   if (Self->ClipPath) {
      // The ClipPath is internal and can be used by the likes of VectorViewport.
      bounding_rect_single(*Self->ClipPath, 0, &bounds[0], &bounds[1], &bounds[2], &bounds[3]);
   }

   if (Self->Child) calc_full_boundary((objVector *)Self->Child, bounds);

   if (bounds[0] >= 1000000) return ERR_Okay; // Return if there are no valid paths.

   LONG width = bounds[2] + 1; // Vector->BX2 - Vector->BX1 + 1;
   LONG height = bounds[3] + 1; // Vector->BY2 - Vector->BY1 + 1;

   if ((width <= 0) OR (height <= 0)) FMSG("@","Warning - invalid mask size of %dx%d detected.", width, height);

   if (width < 0) width = -width;
   else if (!width) width = 1;

   if (height < 0) height = -height;
   else if (!height) height = 1;

   if ((width > 4096) or (height > 4096)) {
      LogErrorMsg("Mask size of %dx%d pixels exceeds imposed limits.", width, height);
      if (width > 4096)  width = 4096;
      if (height > 4096) height = 4096;
   }

   #ifdef DBG_DRAW
      MSG("Drawing clipping mask with bounds %.2f %.2f %.2f %.2f (%dx%d)", bounds[0], bounds[1], bounds[2], bounds[3], width, height);
   #endif

   LONG size = width * height;
   if ((Self->ClipData) AND (size > Self->ClipSize)) {
      FreeMemory(Self->ClipData);
      Self->ClipData = NULL;
      Self->ClipSize = 0;
   }

   if (!Self->ClipData) {
      if (!AllocMemory(size, MEM_DATA|MEM_NO_CLEAR, &Self->ClipData, NULL)) {
         Self->ClipSize = size;
      }
      else return ERR_AllocMemory;
   }

   Self->ClipRenderer->attach(Self->ClipData, width-1, height-1, width);
   agg::pixfmt_gray8 pixf(*Self->ClipRenderer);
   agg::renderer_base<agg::pixfmt_gray8> rb(pixf);
   agg::renderer_scanline_aa_solid<agg::renderer_base<agg::pixfmt_gray8>> solid(rb);
   agg::rasterizer_scanline_aa<> rasterizer;

   ClearMemory(Self->ClipData, Self->ClipSize);

   solid.color(agg::gray8(0xff, 0xff));

   // Every child vector of the VectorClip that exports a path will be rendered to the mask.

   if (Self->Child) draw_clips(Self, (objVector *)Self->Child, rasterizer, solid);

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

//****************************************************************************

static ERROR CLIP_Free(objVectorClip *Self, APTR Void)
{
   struct VectorTransform *scan, *next;
   for (scan=Self->Transforms; scan; scan=next) {
      next = scan->Next;
      FreeMemory(scan);
   }
   Self->Transforms = NULL;

   if (Self->ClipData) { FreeMemory(Self->ClipData); Self->ClipData = NULL; }
   if (Self->ClipPath) { delete Self->ClipPath; Self->ClipPath = NULL; }
   if (Self->ClipRenderer) { delete Self->ClipRenderer; Self->ClipRenderer = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR CLIP_Init(objVectorClip *Self, APTR Void)
{
   if ((Self->ClipUnits <= 0) or (Self->ClipUnits >= VUNIT_END)) {
      FMSG("@","Invalid Units value of %d", Self->ClipUnits);
      return PostError(ERR_OutOfRange);
   }

   if ((!Self->Parent) OR ((Self->Parent->ClassID != ID_VECTORSCENE) AND (Self->Parent->SubID != ID_VECTORVIEWPORT))) {
      LogErrorMsg("This VectorClip object must be a child of a Scene or Viewport object.");
      return ERR_Failed;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR CLIP_NewObject(objVectorClip *Self, APTR Void)
{
   Self->ClipUnits = VUNIT_BOUNDING_BOX;
   Self->ClipRenderer = new (std::nothrow) agg::rendering_buffer;
   if (!Self->ClipRenderer) return ERR_AllocMemory;
   Self->Visibility = VIS_HIDDEN; // Because the content of the clip object must be ignored by the core vector drawing routine.
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Transform: Applies a transform to the paths in the clipping mask.

A transform can be applied to the paths in the clipping mask by setting this field with an SVG compliant transform
string.

*****************************************************************************/

static ERROR CLIP_SET_Transform(objVectorClip *Self, CSTRING Value)
{
   if (!Value) return PostError(ERR_NullArgs);

   // Clear any existing transforms.

   struct VectorTransform *scan, *next;
   for (scan=Self->Transforms; scan; scan=next) {
      next = scan->Next;
      FreeMemory(scan);
   }
   Self->Transforms = NULL;

   struct VectorTransform *transform;

   CSTRING str = Value;
   while (*str) {
      if (!StrCompare(str, "matrix", 6, 0)) {
         if ((transform = add_transform(Self, VTF_MATRIX))) {
            str = read_numseq(str+6, &transform->Matrix[0], &transform->Matrix[1], &transform->Matrix[2], &transform->Matrix[3], &transform->Matrix[4], &transform->Matrix[5], TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "translate", 9, 0)) {
         if ((transform = add_transform(Self, VTF_TRANSLATE))) {
            DOUBLE x = 0;
            DOUBLE y = 0;
            str = read_numseq(str+9, &x, &y, TAGEND);
            transform->X += x;
            transform->Y += y;
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "rotate", 6, 0)) {
         if ((transform = add_transform(Self, VTF_ROTATE))) {
            str = read_numseq(str+6, &transform->Angle, &transform->X, &transform->Y, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "scale", 5, 0)) {
         if ((transform = add_transform(Self, VTF_SCALE))) {
            str = read_numseq(str+5, &transform->X, &transform->Y, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "skewX", 5, 0)) {
         if ((transform = add_transform(Self, VTF_SKEW))) {
            transform->X = 0;
            str = read_numseq(str+5, &transform->X, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else if (!StrCompare(str, "skewY", 5, 0)) {
         if ((transform = add_transform(Self, VTF_SKEW))) {
            transform->Y = 0;
            str = read_numseq(str+5, &transform->Y, TAGEND);
         }
         else return ERR_AllocMemory;
      }
      else str++;
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Units: Defines the coordinate system for fields X, Y, Width and Height.

The default coordinate system for clip-paths is BOUNDING_BOX, which positions the clipping region against the vector
that references it.  The alternative is USERSPACE, which positions the path relative to the current viewport.
-END-
*****************************************************************************/

static ERROR CLIP_GET_Units(objVectorClip *Self, LONG *Value)
{
   *Value = Self->ClipUnits;
   return ERR_Okay;
}

static ERROR CLIP_SET_Units(objVectorClip *Self, LONG Value)
{
   Self->ClipUnits = Value;
   return ERR_Okay;
}

//****************************************************************************

static const struct ActionArray clClipActions[] = {
   { AC_Draw,      (APTR)CLIP_Draw },
   { AC_Free,      (APTR)CLIP_Free },
   { AC_Init,      (APTR)CLIP_Init },
   { AC_NewObject, (APTR)CLIP_NewObject },
   { 0, NULL }
};

static const struct FieldDef clClipUnits[] = {
   { "BoundingBox", VUNIT_BOUNDING_BOX },  // Coordinates are relative to the object's bounding box
   { "UserSpace",   VUNIT_USERSPACE },    // Coordinates are relative to the current viewport
   { NULL, 0 }
};

static const struct FieldArray clClipFields[] = {
   { "Units",     FDF_VIRTUAL|FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clClipUnits, (APTR)CLIP_GET_Units, (APTR)CLIP_SET_Units },
   { "Transform", FDF_VIRTUAL|FDF_STRING|FDF_W, 0, NULL, (APTR)CLIP_SET_Transform },
   END_FIELD
};

static ERROR init_clip(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorClip,
      FID_BaseClassID|TLONG, ID_VECTOR,
      FID_SubClassID|TLONG,  ID_VECTORCLIP,
      FID_Name|TSTRING,      "VectorClip",
      FID_Actions|TPTR,      clClipActions,
      FID_Fields|TARRAY,     clClipFields,
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Size|TLONG,        sizeof(objVectorClip),
      FID_Path|TSTR,         MOD_PATH,
      TAGEND));
}

