
/*********************************************************************************************************************
** Menu class
*/


Scintilla::Menu::Menu() {}

void Scintilla::Menu::CreatePopUp()
{
   parasol::Log log(__FUNCTION__);
   OBJECTID display_id;
   //OBJECTPTR menu;

   log.branch();

   Destroy();

   /*** Create Parasol popup window ***/

   /*OBJECTID surface_id;
   if (CreateObject(ID_SURFACE, 0, NULL, &surface_id,
      FID_Name|TSTRING,    "ScintillaPopup",
      TAGEND) != ERR_Okay) {

      log.warning("Surface creation failed.");
      return;
   }*/

   FindObject((STRING)"SystemSurface", ID_SURFACE, 0, &display_id);

   //id = *reinterpret_cast<MenuID *>(&surface_id);
/*
   if (CreateObject(ID_MENU, 0, &menu,
      FID_Target|TLONG,       display_id,
      //FID_Relative|TLONG,     surface_id,
      //FID_Drawable|TLONG,   surface_id,
      //FID_Font|TPTR,        menufont,
      TAGEND) != ERR_Okay) {

      log.warning("Menu creation failed.");
      return;
   }
*/
}

/****************************************************************************/

void Scintilla::Menu::Destroy()
{
   parasol::Log log(__FUNCTION__);
   log.traceBranch();

   //OBJECTID surface_id;
   //GetField(menu, FID_Drawable, FT_LONG, &surface_id);

   //ActionMsg(AC_Free, *reinterpret_cast<OBJECTID *>(&id), NULL);
   //acFree(menu);

   //if (surface_id)
   //   ActionMsg(AC_Free, surface_id, NULL);
}

/****************************************************************************/

void Scintilla::Menu::Show(Scintilla::Point pt, Window &Window)
{
   parasol::Log log(__FUNCTION__);
   log.branch("%dx%d", pt.x, pt.y);

#if 0
   OBJECTPTR menu;
   OBJECTID winsurface_id;

   winsurface_id = getSurfaceID(&Window);

   SetFields(menu, FID_Relative|TLONG, winsurface_id,
                   FID_X|TLONG,   pt.x,
                   FID_Y|TLONG,   pt.y,
                   TAGEND);

   /* Get the surface ID */
   //OBJECTID surface_id;
   //GetField(menu, FID_Drawable, FT_LONG, &surface_id);

   //AccessSurface(surface_id);
   //acRedimension(ar.GetSurface(), pt.x, pt.y, 0, 200, 200, 0);

   //acShow(ar.GetSurface());

   acShow(menu);


   //AccessSurface ar(*reinterpret_cast<OBJECTID *>(&id));

   /*** Move the window to the correct point ***/

   /*ar.GetSurface()->XCoord = pt.x;
   ar.GetSurface()->YCoord = pt.y;
   ar.GetSurface()->Width = 200;//TEMP
   ar.GetSurface()->Height = 200;*/

   //acRedimension(ar.GetSurface(), pt.x, pt.y, 0, 200, 200, 0);

   /*** Show the window ***/

   //acShow(ar.GetSurface());
#endif
}
