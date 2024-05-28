// SVG renderer for the Picture class

#include "../picture/picture.h"

//********************************************************************************************************************

static ERR RSVG_Activate(extPicture *Self)
{
   prvSVG *prv;
   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR::NotInitialised;

   ERR error;
   if ((error = acQuery(Self)) != ERR::Okay) return error;

   auto bmp = Self->Bitmap;
   if (!bmp->initialised()) {
      if (InitObject(bmp) != ERR::Okay) return ERR::Init;
   }

   gfxDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0, BAF::FILL); // Black background
   svgRender(prv->SVG, bmp, 0, 0, bmp->Width, bmp->Height);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR RSVG_Free(extPicture *Self)
{
   if (auto prv = (prvSVG *)Self->ChildPrivate) {
      if (prv->SVG) { FreeResource(prv->SVG); prv->SVG = NULL; }
   }
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR RSVG_Init(extPicture *Self)
{
   pf::Log log;
   STRING path;

   Self->get(FID_Path, &path);

   if ((!path) or ((Self->Flags & PCF::NEW) != PCF::NIL)) {
      return ERR::NoSupport; // Creating new SVG's is not supported in this module.
   }

   char *buffer;

   if (wildcmp("*.svg|*.svgz", path));
   else if (Self->getPtr(FID_Header, &buffer) IS ERR::Okay) {
      if (StrSearch("<svg", buffer) >= 0) {
      }
      else return ERR::NoSupport;
   }
   else return ERR::NoSupport;

   log.trace("File \"%s\" is in SVG format.", path);

   Self->Flags |= PCF::SCALABLE;

   if (AllocMemory(sizeof(prvSVG), MEM::DATA, &Self->ChildPrivate) IS ERR::Okay) {
      if ((Self->Flags & PCF::LAZY) != PCF::NIL) return ERR::Okay;
      return acActivate(Self);
   }
   else return ERR::AllocMemory;
}

//********************************************************************************************************************

static ERR RSVG_Query(extPicture *Self)
{
   pf::Log log;
   prvSVG *prv;
   objBitmap *bmp;

   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR::NotInitialised;
   if (!(bmp = Self->Bitmap)) return log.warning(ERR::ObjectCorrupt);

   if (Self->Queried) return ERR::Okay;
   Self->Queried = TRUE;

   if (!prv->SVG) {
      STRING path;
      if (Self->get(FID_Path, &path) IS ERR::Okay) {
         if ((prv->SVG = objSVG::create::local(fl::Path(path)))) {
         }
         else return log.warning(ERR::CreateObject);
      }
      else return log.warning(ERR::GetField);
   }

   objVectorScene *scene;
   ERR error;
   if (((error = prv->SVG->getPtr(FID_Scene, &scene)) IS ERR::Okay) and (scene)) {
      if ((Self->Flags & PCF::FORCE_ALPHA_32) != PCF::NIL) {
         bmp->Flags |= BMF::ALPHA_CHANNEL;
         bmp->BitsPerPixel  = 32;
         bmp->BytesPerPixel = 4;
      }

      // Look for the viewport, represented by the <svg/> tag.

      objVector *view = scene->Viewport;
      while ((view) and (view->classID() != CLASSID::VECTORVIEWPORT)) view = view->Next;
      if (!view) {
         log.warning("SVG source file does not define a valid <svg/> tag.");
         return ERR::Failed;
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

static ERR RSVG_Resize(extPicture *Self, struct acResize *Args)
{
   prvSVG *prv;
   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR::NotInitialised;

   if (!Args) return ERR::NullArgs;

   if (prv->SVG) {
      if (!Self->Bitmap->initialised()) {
         if (InitObject(Self->Bitmap) != ERR::Okay) return ERR::Init;
      }

      if (Action(AC_Resize, Self->Bitmap, Args) IS ERR::Okay) {
         objVectorScene *scene;
         if ((prv->SVG->getPtr(FID_Scene, &scene) IS ERR::Okay) and (scene)) {
            scene->setPageWidth(Self->Bitmap->Width);
            scene->setPageHeight(Self->Bitmap->Height);

            gfxDrawRectangle(Self->Bitmap, 0, 0, Self->Bitmap->Width, Self->Bitmap->Height, 0, BAF::FILL);
            acDraw(prv->SVG);
         }
         else return ERR::GetField;

         return ERR::Okay;
      }
      else return ERR::Failed;
   }
   else return ERR::NotInitialised;
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

static ERR init_rsvg(void)
{
   clRSVG = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::PICTURE),
      fl::ClassID(CLASSID::RSVG),
      fl::Name("RSVG"),
      fl::Category(CCF::GRAPHICS),
      fl::FileExtension("*.svg|*.svgz"),
      fl::FileDescription("SVG image"),
      fl::Actions(clActions),
      fl::Path(MOD_PATH));

   return clRSVG ? ERR::Okay : ERR::AddClass;
}
