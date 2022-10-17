
//****************************************************************************

static ERROR RSVG_Activate(objPicture *Self, APTR Void)
{
   prvSVG *prv;
   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR_NotInitialised;

   ERROR error;
   if ((error = acQuery(Self)) != ERR_Okay) return error;

   auto bmp = Self->Bitmap;
   if (!bmp->initialised()) {
      if (acInit(bmp) != ERR_Okay) return ERR_Init;
   }

   gfxDrawRectangle(bmp, 0, 0, bmp->Width, bmp->Height, 0, BAF_FILL); // Black background
   svgRender(prv->SVG, bmp, 0, 0, bmp->Width, bmp->Height);
   return ERR_Okay;
}

//****************************************************************************

static ERROR RSVG_Free(objPicture *Self, APTR Void)
{
   prvSVG *prv;
   if ((prv = (prvSVG *)Self->ChildPrivate)) {
      if (prv->SVG) { acFree(prv->SVG); prv->SVG = NULL; }
   }
   return ERR_Okay;
}

//****************************************************************************

static ERROR RSVG_Init(objPicture *Self, APTR Void)
{
   parasol::Log log;
   STRING path;

   GetString(Self, FID_Path, &path);

   if ((!path) OR (Self->Flags & PCF_NEW)) {
      return ERR_NoSupport; // Creating new SVG's is not supported in this module.
   }

   char *buffer;

   if (!StrCompare("*.svg|*.svgz", path, 0, STR_WILDCARD));
   else if (!GetPointer(Self, FID_Header, &buffer)) {
      if (StrSearch("<svg", buffer, 0) >= 0) {
      }
      else return ERR_NoSupport;
   }
   else return ERR_NoSupport;

   log.trace("File \"%s\" is in SVG format.", path);

   Self->Flags |= PCF_SCALABLE;

   if (!AllocMemory(sizeof(prvSVG), Self->MemFlags, &Self->ChildPrivate, NULL)) {
      if (Self->Flags & PCF_LAZY) return ERR_Okay;
      return acActivate(Self);
   }
   else return ERR_AllocMemory;
}

//****************************************************************************

static ERROR RSVG_Query(objPicture *Self, APTR Void)
{
   parasol::Log log;
   prvSVG *prv;
   objBitmap *bmp;

   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR_NotInitialised;
   if (!(bmp = Self->Bitmap)) return log.warning(ERR_ObjectCorrupt);

   if (Self->Queried) return ERR_Okay;
   Self->Queried = TRUE;

   if (!prv->SVG) {
      STRING path;
      if (!GetString(Self, FID_Path, &path)) {
         if (!CreateObject(ID_SVG, NF_INTEGRAL, &prv->SVG,
               FID_Path|TSTR, path,
               TAGEND)) {
         }
         else return log.warning(ERR_CreateObject);
      }
      else return log.warning(ERR_GetField);
   }

   objVectorScene *scene;
   ERROR error;
   if ((!(error = GetPointer(prv->SVG, FID_Scene, &scene))) AND (scene)) {
      if (Self->Flags & PCF_FORCE_ALPHA_32) {
         bmp->Flags |= BMF_ALPHA_CHANNEL;
         bmp->BitsPerPixel  = 32;
         bmp->BytesPerPixel = 4;
      }

      // Look for the viewport, represented by the <svg/> tag.

      objVector *view = scene->Viewport;
      while ((view) and (view->SubID != ID_VECTORVIEWPORT)) view = view->Next;
      if (!view) {
         log.warning("SVG source file does not define a valid <svg/> tag.");
         return ERR_Failed;
      }

      // Check for fixed dimensions specified by the SVG.

      LONG view_width = 0, view_height = 0;
      GetLong(view, FID_Width, &view_width);
      GetLong(view, FID_Height, &view_height);

      // If the SVG source doesn't specify fixed dimensions, automatically force rescaling to the display width and height.

      if (!view_width)  SetField(view, FID_Width|TDOUBLE|TPERCENT, 100.0);
      if (!view_height) SetField(view, FID_Height|TDOUBLE|TPERCENT, 100.0);

      if ((Self->DisplayWidth > 0) AND (Self->DisplayHeight > 0)) { // Client specified the display size?
         // Give the vector scene a target width and height.
         if (!view_width) SetLong(scene, FID_PageWidth, Self->DisplayWidth);
         else SetLong(scene, FID_PageWidth, view_width);

         if (!view_height) SetLong(scene, FID_PageHeight, Self->DisplayHeight);
         else SetLong(scene, FID_PageHeight, view_height);
      }

      if (!bmp->Width) {
         if (view_width) bmp->Width = view_width;
         else if (Self->DisplayWidth) bmp->Width = Self->DisplayWidth;
         if (!bmp->Width) bmp->Width = 1024;
      }

      if (!bmp->Height) {
         if (view_height) bmp->Height = view_height;
         else if (Self->DisplayHeight) bmp->Height = Self->DisplayHeight;
         if (!bmp->Height) bmp->Height = 768;
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

//****************************************************************************

static ERROR RSVG_Resize(objPicture *Self, struct acResize *Args)
{
   prvSVG *prv;
   if (!(prv = (prvSVG *)Self->ChildPrivate)) return ERR_NotInitialised;

   if (!Args) return ERR_NullArgs;

   if (prv->SVG) {
      if (!Self->Bitmap->initialised()) {
         if (acInit(Self->Bitmap)) return ERR_Init;
      }

      if (!Action(AC_Resize, Self->Bitmap, Args)) {
         objVectorScene *scene;
         if ((!GetPointer(prv->SVG, FID_Scene, &scene)) AND (scene)) {
            SetLong(scene, FID_PageWidth, Self->Bitmap->Width);
            SetLong(scene, FID_PageHeight, Self->Bitmap->Height);

            gfxDrawRectangle(Self->Bitmap, 0, 0, Self->Bitmap->Width, Self->Bitmap->Height, 0, TRUE);
            acDraw(prv->SVG);
         }
         else return ERR_GetField;

         return ERR_Okay;
      }
      else return ERR_Failed;
   }
   else return ERR_NotInitialised;
}

//****************************************************************************

static const ActionArray clActions[] = {
   { AC_Activate, (APTR)RSVG_Activate },
   { AC_Free,     (APTR)RSVG_Free },
   { AC_Init,     (APTR)RSVG_Init },
   { AC_Query,    (APTR)RSVG_Query },
   { AC_Resize,   (APTR)RSVG_Resize },
   { 0, NULL }
};

//****************************************************************************

static ERROR init_rsvg(void)
{
   return CreateObject(ID_METACLASS, 0, &clRSVG,
      FID_BaseClassID|TLONG,    ID_PICTURE,
      FID_SubClassID|TLONG,     ID_RSVG,
      FID_Name|TSTRING,         "RSVG",
      FID_Category|TLONG,       CCF_GRAPHICS,
      FID_FileExtension|TSTR,   "*.svg|*.svgz",
      FID_FileDescription|TSTR, "SVG image",
      FID_Actions|TPTR,         clActions,
      FID_Path|TSTR,            MOD_PATH,
      TAGEND);
}
