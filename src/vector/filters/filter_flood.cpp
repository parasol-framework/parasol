
//****************************************************************************
// Create a new flood effect.

static ERROR create_flood(objVectorFilter *Self, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);

   auto effect_it = Self->Effects->emplace(Self->Effects->end(), FE_FLOOD);
   auto &effect = *effect_it;

   // Dimensions are relative to the VectorFilter's Bound* dimensions.

   effect.Flood.Dimensions = DMF_RELATIVE_X|DMF_RELATIVE_Y|DMF_RELATIVE_WIDTH|DMF_RELATIVE_HEIGHT;
   effect.Flood.X = 0;
   effect.Flood.Y = 0;
   effect.Flood.Width = 1.0;
   effect.Flood.Height = 1.0;
   effect.Flood.Opacity = 1.0;

   for (LONG a=1; a < Tag->TotalAttrib; a++) {
      CSTRING val = Tag->Attrib[a].Value;
      if (!val) continue;

      UBYTE percent;
      ULONG hash = StrHash(Tag->Attrib[a].Name, FALSE);
      switch(hash) {
         case SVF_X:
            effect.Flood.X = read_unit(val, &percent);
            if (percent) effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_FIXED_X)) | DMF_RELATIVE_X;
            else effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_RELATIVE_X)) | DMF_FIXED_X;
            break;
         case SVF_Y:
            effect.Flood.Y = read_unit(val, &percent);
            if (percent) effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_FIXED_Y)) | DMF_RELATIVE_Y;
            else effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_RELATIVE_Y)) | DMF_FIXED_Y;
            break;
         case SVF_WIDTH:
            effect.Flood.Width = read_unit(val, &percent);
            if (percent) effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_FIXED_WIDTH)) | DMF_RELATIVE_WIDTH;
            else effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_RELATIVE_WIDTH)) | DMF_FIXED_WIDTH;
            break;
         case SVF_HEIGHT:
            effect.Flood.Height = read_unit(val, &percent);
            if (percent) effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_FIXED_HEIGHT)) | DMF_RELATIVE_HEIGHT;
            else effect.Flood.Dimensions = (effect.Flood.Dimensions & (~DMF_RELATIVE_HEIGHT)) | DMF_FIXED_HEIGHT;
            break;
         case SVF_FLOOD_COLOR:
         case SVF_FLOOD_COLOUR: {
            struct DRGB frgb;
            vecReadPainter((OBJECTPTR)NULL, val, &frgb, NULL, NULL, NULL);
            effect.Flood.Colour.Red   = F2I(frgb.Red * 255.0);
            effect.Flood.Colour.Green = F2I(frgb.Green * 255.0);
            effect.Flood.Colour.Blue  = F2I(frgb.Blue * 255.0);
            effect.Flood.Colour.Alpha = F2I(frgb.Alpha * 255.0);
            break;
         }
         case SVF_FLOOD_OPACITY: read_numseq(val, &effect.Flood.Opacity, TAGEND); break;
         default: fe_default(Self, &effect, hash, val); break;
      }
   }

   effect.Flood.Colour.Alpha = F2I((DOUBLE)effect.Flood.Colour.Alpha * effect.Flood.Opacity);

   if (!effect.Flood.Colour.Alpha) {
      log.warning("A valid flood-colour is required.");
      Self->Effects->erase(effect_it);
      return ERR_Failed;
   }

   return ERR_Okay;
}

//****************************************************************************
// This is the stack flood algorithm originally implemented in AGG.

static void apply_flood(objVectorFilter *Self, VectorEffect *Effect)
{
   auto bmp = Effect->Bitmap;
   if (bmp->BytesPerPixel != 4) return;

   LONG x, y, width, height;

   if (Effect->Flood.Dimensions & DMF_RELATIVE_X) x = (DOUBLE)Self->BoundWidth * Effect->Flood.X;
   else x = Effect->Flood.X;

   if (Effect->Flood.Dimensions & DMF_RELATIVE_Y) y = (DOUBLE)Self->BoundHeight * Effect->Flood.Y;
   else y = Effect->Flood.Y;

   if (Effect->Flood.Dimensions & DMF_RELATIVE_WIDTH) width = (DOUBLE)Self->BoundWidth * Effect->Flood.Width;
   else width = Effect->Flood.Width;

   if (Effect->Flood.Dimensions & DMF_RELATIVE_HEIGHT) height = (DOUBLE)Self->BoundHeight * Effect->Flood.Height;
   else height = F2I(Effect->Flood.Height);

   ULONG colour = PackPixelWBA(bmp, Effect->Flood.Colour.Red, Effect->Flood.Colour.Green, Effect->Flood.Colour.Blue, Effect->Flood.Colour.Alpha);
   gfxDrawRectangle(bmp, bmp->Clip.Left+x, bmp->Clip.Top+y, width, height, colour, BAF_FILL);
}
