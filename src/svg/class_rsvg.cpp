// SVG renderer for the Picture class

#include "../picture/picture.h"

//********************************************************************************************************************

static ERROR RSVG_Activate(extPicture *Self, APTR Void)
{
   prvSVG *prv;
   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR_NotInitialised;

   ERROR error;
   if ((error = acQuery(Self)) != ERR_Okay) return error;

   auto bmp = Self->Bitmap;
   if (!bmp->initialised()) {
      if (InitObject(bmp) != ERR_Okay) return ERR_Init;
   }

   gfxDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0, BAF::FILL); // Black background
   svgRender(prv->SVG, bmp, 0, 0, bmp->Width, bmp->Height);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR RSVG_Free(extPicture *Self, APTR Void)
{
   if (auto prv = (prvSVG *)Self->ChildPrivate) {
      if (prv->SVG) { FreeResource(prv->SVG); prv->SVG = NULL; }
   }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR RSVG_Init(extPicture *Self, APTR Void)
{
   pf::Log log;
   STRING path;

   Self->get(FID_Path, &path);

   if ((!path) or ((Self->Flags & PCF::NEW) != PCF::NIL)) {
      return ERR_NoSupport; // Creating new SVG's is not supported in this module.
   }

   char *buffer;

   if (!StrCompare("*.svg|*.svgz", path, 0, STR::WILDCARD));
   else if (!Self->getPtr(FID_Header, &buffer)) {
      if (StrSearch("<svg", buffer) >= 0) {
      }
      else return ERR_NoSupport;
   }
   else return ERR_NoSupport;

   log.trace("File \"%s\" is in SVG format.", path);

   Self->Flags |= PCF::SCALABLE;

   if (!AllocMemory(sizeof(prvSVG), MEM::DATA, &Self->ChildPrivate)) {
      if ((Self->Flags & PCF::LAZY) != PCF::NIL) return ERR_Okay;
      return acActivate(Self);
   }
   else return ERR_AllocMemory;
}

//********************************************************************************************************************

static ERROR RSVG_Query(extPicture *Self, APTR Void)
{
   pf::Log log;
   prvSVG *prv;
   objBitmap *bmp;

   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR_NotInitialised;
   if (!(bmp = Self->Bitmap)) return log.warning(ERR_ObjectCorrupt);

   if (Self->Queried) return ERR_Okay;
   Self->Queried = TRUE;

   if (!prv->SVG) {
      STRING path;
      if (!Self->get(FID_Path, &path)) {
         if ((prv->SVG = objSVG::create::integral(fl::Path(path)))) {
         }
         else return log.warning(ERR_CreateObject);
      }
      else return log.warning(ERR_GetField);
   }

   objVectorScene *scene;
   ERROR error;
   if ((!(error = prv->SVG->getPtr(FID_Scene, &scene))) and (scene)) {
      if ((Self->Flags & PCF::FORCE_ALPHA_32) != PCF::NIL) {
         bmp->Flags |= BMF::ALPHA_CHANNEL;
         bmp->BitsPerPixel  = 32;
         bmp->BytesPerPixel = 4;
      }

      // Look for the viewport, represented by the <svg/> tag.

      objVector *view = scene->Viewport;
      while ((view) and (view->Class->ClassID != ID_VECTORVIEWPORT)) view = view->Next;
      if (!view) {
         log.warning("SVG source file does not define a valid <svg/> tag.");
         return ERR_Failed;
      }

      // Check for fixed dimensions specified by the SVG.

      auto view_width = view->get<LONG>(FID_Width);
      auto view_height = view->get<LONG>(FID_Height);

      // If the SVG source doesn't specify fixed dimensions, automatically force rescaling to the display width and height.

      if (!view_width)  SetField(view, FID_Width|TDOUBLE|TSCALE, 1.0);
      if (!view_height) SetField(view, FID_Height|TDOUBLE|TSCALE, 1.0);

      if ((Self->DisplayWidth > 0) and (Self->DisplayHeight > 0)) { // Client specified the display size?
         // Give the vector scene a target width and height.
         if (!view_width) scene->setPageWidth(Self->DisplayWidth);
         else scene->setPageWidth(view_width);

         if (!view_height) scene->setPageHeight(Self->DisplayHeight);
         else scene->setPageHeight(view_height);
      }

      if (!bmp->Width) {
         if (view_width) bmp->Width = view_width;
         else if (Self->DisplayWidth) bmp->Width = Self->DisplayWidth;
         if (!bmp->Width) bmp->Width = 1024;
      }

      if (!bmp->Height) {
         if (view_height) bmp->Height = view_height;
         else if (Self->DisplayHeight) bmp->Height = Self->DisplayHeight;
         if (!bmp->Height) bmp->Height = bmp->Width; // Equivalent to width in order to maintain a 1:1 scale
      }

      if (!Self->DisplayWidth)  Self->DisplayWidth  = bmp->Width;
      if (!Self->DisplayHeight) Self->DisplayHeight = bmp->Height;
      if (bmp->BitsPerPixel < 15) bmp->BitsPerPixel = 32;

      error = acQuery(bmp);
      return error;
   }
   else {
      log.trace("Failed to retrieve Vector from SVG.");
      return log.warning(error);
   }
}

//********************************************************************************************************************

static ERROR RSVG_Resize(extPicture *Self, struct acResize *Args)
{
   prvSVG *prv;
   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR_NotInitialised;

   if (!Args) return ERR_NullArgs;

   if (prv->SVG) {
      if (!Self->Bitmap->initialised()) {
         if (InitObject(Self->Bitmap)) return ERR_Init;
      }

      if (!Action(AC_Resize, Self->Bitmap, Args)) {
         objVectorScene *scene;
         if ((!prv->SVG->getPtr(FID_Scene, &scene)) and (scene)) {
            scene->setPageWidth(Self->Bitmap->Width);
            scene->setPageHeight(Self->Bitmap->Height);

            gfxDrawRectangle(Self->Bitmap, 0, 0, Self->Bitmap->Width, Self->Bitmap->Height, 0, BAF::FILL);
            acDraw(prv->SVG);
         }
         else return ERR_GetField;

         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else return ERR_NotInitialised;
}

//********************************************************************************************************************

static const ActionArray clActions[] = {
   { AC_Activate, RSVG_Activate },
   { AC_Free,     RSVG_Free },
   { AC_Init,     RSVG_Init },
   { AC_Query,    RSVG_Query },
   { AC_Resize,   RSVG_Resize },
   { 0, NULL }
};

//********************************************************************************************************************

static ERROR init_rsvg(void)
{
   clRSVG = objMetaClass::create::global(
      fl::BaseClassID(ID_PICTURE),
      fl::ClassID(ID_RSVG),
      fl::Name("RSVG"),
      fl::Category(CCF::GRAPHICS),
      fl::FileExtension("*.svg|*.svgz"),
      fl::FileDescription("SVG image"),
      fl::Actions(clActions),
      fl::Path(MOD_PATH));

   return clRSVG ? ERR_Okay : ERR_AddClass;
}
