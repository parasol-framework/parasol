/*********************************************************************************************************************

-CLASS-
SVG: Provides support for parsing and rendering SVG files.

The SVG class provides support for parsing SVG statements into native @Vector objects and related definitions.
For low-level vector programming, consider using the @Vector class directly, or use the SVG class to parse an
SVG script and then access the #Viewport field to perform transforms and manipulation of the vector group.

Please refer to the W3C documentation on SVG for a complete reference to the attributes that can be applied to SVG
elements.  Unfortunately we do not support all SVG capabilities at this time, but support will improve in future.

*********************************************************************************************************************/

static void notify_free_frame_callback(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   ((extSVG *)CurrentContext())->FrameCallback.Type = CALL_NONE;
}

/*********************************************************************************************************************
-ACTION-
Activate: Initiates playback of SVG animations.

SVG documents that use animation features will remain static until they are activated with this action.  The animation
code will be processed in the background at the pre-defined #FrameRate.  The client may hook into the animation cycle
by setting the #FrameCallback with a suitable function.

-END-
*********************************************************************************************************************/

static ERROR SVG_Activate(extSVG *Self, APTR Void)
{
   if (Self->Animated) {
      if (!Self->AnimationTimer) {
         auto call = make_function_stdc(animation_timer);
         SubscribeTimer(1.0 / (DOUBLE)Self->FrameRate, &call, &Self->AnimationTimer);
      }
      else UpdateTimer(Self->AnimationTimer, 1.0 / (DOUBLE)Self->FrameRate);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Deactivate: Stops all playback of SVG animations.
-END-
*********************************************************************************************************************/

static ERROR SVG_Deactivate(extSVG *Self, APTR Void)
{
   if (Self->AnimationTimer) { UpdateTimer(Self->AnimationTimer, 0); Self->AnimationTimer = 0; }
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: Vector graphics are created by passing XML-based instructions here.
-END-
*********************************************************************************************************************/

static ERROR SVG_DataFeed(extSVG *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Datatype IS DATA_XML) {
      return load_svg(Self, 0, (CSTRING)Args->Buffer);
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SVG_Free(extSVG *Self, APTR Void)
{
   Self->~extSVG();

   if (Self->AnimationTimer) {
      UpdateTimer(Self->AnimationTimer, 0);
      Self->AnimationTimer = 0;
   }

   if (Self->FrameCallback.Type IS CALL_SCRIPT) {
      UnsubscribeAction(Self->FrameCallback.Script.Script, AC_Free);
      Self->FrameCallback.Type = CALL_NONE;
   }

   if ((Self->Target) and (Self->Target IS Self->Scene) and (Self->Scene->ownerID() IS Self->UID)) {
      FreeResource(Self->Target);
      Self->Target = NULL;
   }

   if (Self->Folder) { FreeResource(Self->Folder); Self->Folder = NULL; }
   if (Self->Path)   { FreeResource(Self->Path);   Self->Path = NULL; }
   if (Self->Title)  { FreeResource(Self->Title);  Self->Title = NULL; }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Init: Initialise the SVG object.

Initialising an SVG object will load an SVG source file if a #Path has been specified.  The default behaviour is to
generate the content in a local #Scene object, or alternatively the content can be redirected to an external
@VectorScene referred to by #Target.

-END-
*********************************************************************************************************************/

static ERROR SVG_Init(extSVG *Self, APTR Void)
{
   if (!Self->Target) {
      if ((Self->Target = objVectorScene::create::integral())) {
         Self->Scene = (objVectorScene *)Self->Target;
      }
      else return ERR_NewObject;
   }

   if (Self->Path) return load_svg(Self, Self->Path, NULL);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR SVG_NewObject(extSVG *Self, APTR Void)
{
   #ifdef __ANDROID__
      Self->FrameRate = 30; // Choose a lower frame rate for Android devices, so as to minimise power consumption.
   #else
      Self->FrameRate = 60;
   #endif
   new (Self) extSVG;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
Render: Render the scene to a target Bitamp.

This method will render the vector scene directly to a target bitmap at coordinates (X,Y) and scaled to the desired
(Width,Height).

-INPUT-
obj(Bitmap) Bitmap: The target bitmap.
int X: Target X coordinate.
int Y: Target Y coordinate.
int Width: Target page width.
int Height: Target page height.

-RESULT-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR SVG_Render(extSVG *Self, struct svgRender *Args)
{
   if (!Args) return ERR_NullArgs;

   objBitmap *bmp = Args->Bitmap;
   LONG page_width = Args->Width;
   LONG page_height = Args->Height;

   Self->Scene->setBitmap(bmp);

   Self->Scene->setPageWidth(page_width);
   Self->Scene->setPageHeight(page_height);

//   Self->Scene->Viewport->setViewX(Args->X);
//   Self->Scene->Viewport->setViewY(Args->Y);

   bmp->XOffset += Args->X;
   bmp->YOffset += Args->Y;

   Action(AC_Draw, Self->Scene, NULL);

   bmp->XOffset -= Args->X;
   bmp->YOffset -= Args->Y;

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveImage: Saves the SVG document as a PNG image.

This action will render the SVG document to a bitmap and save the resulting image.  The size of the image is
determined by the PageWidth and PageHeight of the #Scene, or if not defined then the default of 1920x1080 is applied.

The image will be saved in PNG format by default, but can be changed by specifying an alternate ClassID.  PNG
is recommended in particular because it supports an alpha channel.

-END-
*********************************************************************************************************************/

static ERROR SVG_SaveImage(extSVG *Self, struct acSaveImage *Args)
{
   ERROR error;

   if (!Args) return ERR_NullArgs;

   LONG width = 0;
   LONG height = 0;
   Self->Scene->get(FID_PageWidth, &width);
   Self->Scene->get(FID_PageHeight, &height);

   if (!width) width = 1920;
   if (!height) height = 1080;

   objPicture::create pic = { fl::Width(width), fl::Height(height), fl::Flags(PCF_ALPHA|PCF_NEW) };
   if (pic.ok()) {
      if (!(error = svgRender(Self, pic->Bitmap, 0, 0, width, height))) {
         if (!(error = acSaveImage(*pic, Args->Dest, Args->ClassID))) {
            return ERR_Okay;
         }
      }
   }
   else error = ERR_CreateObject;

   return error;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves the SVG document to a data object.
-END-
*********************************************************************************************************************/

static ERROR SVG_SaveToObject(extSVG *Self, struct acSaveToObject *Args)
{
   pf::Log log;
   static char header[] =
"<?xml version=\"1.0\" standalone=\"no\"?>\n\
<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
   ERROR (**routine)(OBJECTPTR, APTR);

   if (!Self->Viewport) return log.warning(ERR_NoData);

   if ((Args->ClassID) and (Args->ClassID != ID_SVG)) {
      auto mc = (objMetaClass *)FindClass(Args->ClassID);
      if ((!mc->getPtr(FID_ActionTable, &routine)) and (routine)) {
         if ((routine[AC_SaveToObject]) and (routine[AC_SaveToObject] != (APTR)SVG_SaveToObject)) {
            return routine[AC_SaveToObject](Self, Args);
         }
         else if ((routine[AC_SaveImage]) and (routine[AC_SaveImage] != (APTR)SVG_SaveImage)) {
            struct acSaveImage saveimage = { .Dest = Args->Dest };
            return routine[AC_SaveImage](Self, &saveimage);
         }
         else return log.warning(ERR_NoSupport);
      }
      else return log.warning(ERR_GetField);
   }
   else {
      objXML::create xml = { fl::Flags(XMF_NEW|XMF_READABLE) };

      if (xml.ok()) {
         ERROR error = xmlInsertXML(*xml, 0, 0, header, NULL);
         LONG index = xml->Tags.back().ID;

         XMLTag *tag;
         if (!(error = xmlInsertXML(*xml, index, XMI_NEXT, "<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:parasol=\"http://www.parasol.ws/xmlns/svg\"/>", &tag))) {
            bool multiple_viewports = (Self->Scene->Viewport->Next) ? true : false;
            if (multiple_viewports) {
               if (!(error = save_svg_defs(Self, *xml, Self->Scene, index))) {
                  for (auto scan=Self->Scene->Viewport; scan; scan=(objVectorViewport *)scan->Next) {
                     if (!scan->Child) continue; // Ignore dummy viewports with no content
                     save_svg_scan(Self, *xml, scan, index);
                  }

                  error = xml->saveToObject(Args->Dest);
               }
            }
            else {
               DOUBLE x, y, width, height;

               if (!error) error = Self->Viewport->get(FID_ViewX, &x);
               if (!error) error = Self->Viewport->get(FID_ViewY, &y);
               if (!error) error = Self->Viewport->get(FID_ViewWidth, &width);
               if (!error) error = Self->Viewport->get(FID_ViewHeight, &height);

               if (!error) {
                  char buffer[80];
                  snprintf(buffer, sizeof(buffer), "%g %g %g %g", x, y, width, height);
                  xmlNewAttrib(tag, "viewBox", buffer);
               }

               LONG dim;
               if ((!error) and (!(error = Self->Viewport->get(FID_Dimensions, &dim)))) {
                  if ((dim & (DMF_RELATIVE_X|DMF_FIXED_X)) and (!Self->Viewport->get(FID_X, &x)))
                     set_dimension(tag, "x", x, dim & DMF_RELATIVE_X);

                  if ((dim & (DMF_RELATIVE_Y|DMF_FIXED_Y)) and (!Self->Viewport->get(FID_Y, &y)))
                     set_dimension(tag, "y", y, dim & DMF_RELATIVE_Y);

                  if ((dim & (DMF_RELATIVE_WIDTH|DMF_FIXED_WIDTH)) and (!Self->Viewport->get(FID_Width, &width)))
                     set_dimension(tag, "width", width, dim & DMF_RELATIVE_WIDTH);

                  if ((dim & (DMF_RELATIVE_HEIGHT|DMF_FIXED_HEIGHT)) and (!Self->Viewport->get(FID_Height, &height)))
                     set_dimension(tag, "height", height, dim & DMF_RELATIVE_HEIGHT);
               }

               if (!error) {
                  if (!(error = save_svg_defs(Self, *xml, Self->Scene, index))) {
                     for (auto scan=((objVector *)Self->Viewport)->Child; scan; scan=scan->Next) {
                        save_svg_scan(Self, *xml, scan, index);
                     }

                     error = xml->saveToObject(Args->Dest);
                  }
               }
            }
         }

         return error;
      }
      else return ERR_CreateObject;
   }
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Frame: Forces the graphics to be drawn to a specific frame.

If this field is set to a valid frame number, the vector graphics will only be drawn when the frame of the container
matches the Frame number in this field.  When set to 0 (the default), the Vector will be drawn regardless of the
container's frame number.

-FIELD-
FrameCallback: Optional callback that is triggered whenever a new frame is prepared.

Referencing a function in this field will allow the client to receive a callback after the preparation of each
animation frame (if the SVG object is being animated).  This feature is commonly used to render the SVG document to a
target @Bitmap.

Note that if the SVG document does not make use of any animation features then the function will never be called.

The function prototype is `void Function(*SVG)`.

*********************************************************************************************************************/

static ERROR GET_FrameCallback(extSVG *Self, FUNCTION **Value)
{
   if (Self->FrameCallback.Type != CALL_NONE) {
      *Value = &Self->FrameCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_FrameCallback(extSVG *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->FrameCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->FrameCallback.Script.Script, AC_Free);
      Self->FrameCallback = *Value;
      if (Self->FrameCallback.Type IS CALL_SCRIPT) {
         auto callback = make_function_stdc(notify_free_frame_callback);
         SubscribeAction(Self->FrameCallback.Script.Script, AC_Free, &callback);
      }
   }
   else Self->FrameCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
FrameRate: The maximum frame rate to use when animating a vector scene.

This field defines the maximum frame rate that will be used when a vector scene is animated.  It is recommended that
a value between 50 and 100 is used.  It is important to note that while higher frame rates produce smoother animations,
they also increase the CPU usage proportionately.  For instance, a frame rate of 100 will use the CPU twice as much as
a frame rate of 50.  This will subsequently have an effect on power consumption.

The recommended frame rate is 60, as this will match the majority of modern displays.

*********************************************************************************************************************/

static ERROR SET_FrameRate(extSVG *Self, LONG Value)
{
   if ((Value >= 20) and (Value <= 1000)) {
      Self->FrameRate = Value;
      return ERR_Okay;
   }
   else return ERR_OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
Path: The location of the source SVG data.

SVG data can be loaded from a file source by setting the Path field to an SVG file.

*********************************************************************************************************************/

static ERROR GET_Path(extSVG *Self, STRING *Value)
{
   *Value = Self->Path;
   return ERR_Okay;
}

static ERROR SET_Path(extSVG *Self, CSTRING Value)
{
   if (Self->Path)   { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Folder) { FreeResource(Self->Folder); Self->Folder = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->Path = StrClone(Value))) return ERR_AllocMemory;
   }
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Scene: The VectorScene that manages the Target object is referenced here.

The Scene is a read-only field that assists in quickly finding the @VectorScene that owns the #Target object.

*********************************************************************************************************************/

static ERROR GET_Scene(extSVG *Self, objVectorScene **Value)
{
   *Value = Self->Scene;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Target: The root Viewport that is generated during SVG initialisation can be created as a child of this target object.

During the initialisation of an SVG object, a VectorViewport will be created that hosts the SVG's vector objects.  The
target of this VectorViewport can be specified here, conditional on that object residing within a @VectorScene, or is a
VectorScene itself.

An attempt will be made to find the new target's parent VectorScene.  If none is identified, an error will be returned
and no further action is taken.

If a SVG object is initialised with no Target being defined, a @VectorScene will be created automatically and
referenced by the Target field.

*********************************************************************************************************************/

static ERROR SET_Target(extSVG *Self, OBJECTPTR Value)
{
   if (Value->Class->ClassID IS ID_VECTORSCENE) {
      Self->Target = Value;
      Self->Scene = (objVectorScene *)Value;
      if (Self->Scene->Viewport) Self->Viewport = Self->Scene->Viewport;
   }
   else {
      auto owner_id = Value->ownerID();
      while ((owner_id) and (GetClassID(owner_id) != ID_VECTORSCENE)) {
         owner_id = GetOwnerID(owner_id);
      }

      if (!owner_id) return ERR_Failed;

      Self->Scene = (objVectorScene *)GetObjectPtr(owner_id);
      Self->Target = Value;
      if (Self->Scene->Viewport) Self->Viewport = Self->Scene->Viewport;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Title: The title of the SVG document.

The title of an SVG document is declared with a title element that can embedded anywhere in the document.  In cases
where a title has been specified, it will be possible to read it from this field.  If no title is in the document then
NULL will be returned.

*********************************************************************************************************************/

static ERROR SET_Title(extSVG *Self, CSTRING Value)
{
   if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
   if (Value) Self->Title = StrClone(Value);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Viewport: Returns the first viewport created by an SVG document.

This field simplifies the process of finding the first @VectorViewport that was created by a loaded SVG document.  NULL
is returned if an SVG document has not been successfully parsed yet.
-END-

*********************************************************************************************************************/

static ERROR GET_Viewport(extSVG *Self, OBJECTPTR *Value)
{
   if (!Self->initialised()) return ERR_NotInitialised;
   *Value = Self->Viewport;
   return ERR_Okay;
}

//********************************************************************************************************************

#include "animation.cpp"
#include "gradients.cpp"
#include "parser.cpp"

//********************************************************************************************************************

#include "class_svg_def.c"

static const FieldArray clSVGFields[] = {
   { "Target",    FDF_OBJECT|FDF_RI, NULL, SET_Target },
   { "Path",      FDF_STRING|FDF_RW, GET_Path, SET_Path },
   { "Title",     FDF_STRING|FDF_RW, NULL, SET_Title },
   { "Frame",     FDF_LONG|FDF_RW, NULL, NULL },
   { "Flags",     FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clSVGFlags },
   { "FrameRate", FDF_LONG|FDF_RW, NULL, SET_FrameRate },
   { "FrameCallback", FDF_FUNCTION|FDF_RW, GET_FrameCallback, SET_FrameCallback },
   // Virtual Fields
   { "Src",      FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, GET_Path, SET_Path },
   { "Scene",    FDF_VIRTUAL|FDF_OBJECT|FDF_R, GET_Scene, NULL },
   { "Viewport", FDF_VIRTUAL|FDF_OBJECT|FDF_R, GET_Viewport, NULL },
   END_FIELD
};

static ERROR init_svg(void)
{
   clSVG = objMetaClass::create::global(
      fl::ClassVersion(VER_SVG),
      fl::Name("SVG"),
      fl::Category(CCF::GUI),
      fl::Actions(clSVGActions),
      fl::Methods(clSVGMethods),
      fl::Fields(clSVGFields),
      fl::Flags(CLF::PROMOTE_INTEGRAL),
      fl::Size(sizeof(extSVG)),
      fl::Path(MOD_PATH));

   return clSVG ? ERR_Okay : ERR_AddClass;
}
