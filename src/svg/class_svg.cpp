/*********************************************************************************************************************

-CLASS-
SVG: Provides support for parsing and rendering SVG files.

The SVG class provides support for parsing SVG statements into a scene graph that consists of @Vector objects and
related constructs.  The generated scene graph is accessible via the #Scene and #Viewport fields.

It is possible to parse SVG documents directly to the UI.  Set the #Target field with a vector to contain the SVG
content and it will be structured in the existing scene graph.

Please refer to the W3C documentation on SVG for a complete reference to the attributes that can be applied to SVG
elements.

*********************************************************************************************************************/

static void notify_free_frame_callback(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   ((extSVG *)CurrentContext())->FrameCallback.clear();
}

//********************************************************************************************************************

static void notify_free_scene(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   auto Self = (extSVG *)CurrentContext();
   if (Self->AnimationTimer) { UpdateTimer(Self->AnimationTimer, 0); Self->AnimationTimer = NULL; }
}

/*********************************************************************************************************************
-ACTION-
Activate: Initiates playback of SVG animations.

SVG documents that use animation features will remain static until they are activated with this action.  The animation
code will be processed in the background at the pre-defined #FrameRate.  The #Scene will be redrawn automatically as
each frame is processed.

The client can hook into the animation cycle by setting the #FrameCallback with a suitable function.

-END-
*********************************************************************************************************************/

static ERR SVG_Activate(extSVG *Self, APTR Void)
{
   if (Self->Animated) {
      if (!Self->AnimationTimer) {
         SubscribeTimer(1.0 / (DOUBLE)Self->FrameRate, C_FUNCTION(animation_timer), &Self->AnimationTimer);
         SubscribeAction(Self->Scene, AC_Free, C_FUNCTION(notify_free_scene));
      }
      else UpdateTimer(Self->AnimationTimer, 1.0 / (DOUBLE)Self->FrameRate);
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Deactivate: Stops all playback of SVG animations.
-END-
*********************************************************************************************************************/

static ERR SVG_Deactivate(extSVG *Self, APTR Void)
{
   if (Self->AnimationTimer) { UpdateTimer(Self->AnimationTimer, 0); Self->AnimationTimer = 0; }
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: SVG data can be parsed on-the-fly via the data feed mechanism.
-END-
*********************************************************************************************************************/

static ERR SVG_DataFeed(extSVG *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR::NullArgs;

   if (Args->Datatype IS DATA::XML) {
      return load_svg(Self, 0, (CSTRING)Args->Buffer);
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR SVG_Free(extSVG *Self, APTR Void)
{
   Self->~extSVG();

   if (Self->AnimationTimer) {
      UpdateTimer(Self->AnimationTimer, 0);
      Self->AnimationTimer = 0;
   }

   if (Self->FrameCallback.isScript()) {
      UnsubscribeAction(Self->FrameCallback.Context, AC_Free);
      Self->FrameCallback.clear();
   }

   if ((Self->Target) and (Self->Target IS Self->Scene) and (Self->Scene->Owner IS Self)) {
      FreeResource(Self->Target);
      Self->Target = NULL;
   }

   if (Self->Folder)    { FreeResource(Self->Folder);    Self->Folder = NULL; }
   if (Self->Path)      { FreeResource(Self->Path);      Self->Path = NULL; }
   if (Self->Title)     { FreeResource(Self->Title);     Self->Title = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Init: Initialise the SVG object.

Initialising an SVG object will load an SVG source file if a #Path has been specified.  The default behaviour is to
generate the content in a local #Scene object, or alternatively the content can be redirected to an external
@VectorScene referred to by #Target.

-END-
*********************************************************************************************************************/

static ERR SVG_Init(extSVG *Self, APTR Void)
{
   if (!Self->Target) {
      if ((Self->Target = objVectorScene::create::integral())) {
         Self->Scene = (objVectorScene *)Self->Target;
      }
      else return ERR::NewObject;
   }

   if (Self->Path) return load_svg(Self, Self->Path, NULL);
   else if (Self->Statement) return load_svg(Self, NULL, Self->Statement);

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR SVG_NewObject(extSVG *Self, APTR Void)
{
   #ifdef __ANDROID__
      Self->FrameRate = 30; // Choose a lower frame rate for Android devices, so as to minimise power consumption.
   #else
      Self->FrameRate = 60;
   #endif
   new (Self) extSVG;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
ParseSymbol: Generate a vector scene graph from an SVG symbol, targeting a viewport.

ParseSymbol() allows the symbols of a loaded SVG document to be processed post-initialisation.  This is useful for
utilising symbols in a way that is akin to running macros as required by the program.

The Name must refer to a symbol that has been declared in the loaded document.  A @VectorViewport must be provided
for the symbol's generated content to target.

-INPUT-
cstr ID: Name of the symbol to parse.
obj(VectorViewport) Viewport: The target viewport.

-RESULT-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERR SVG_ParseSymbol(extSVG *Self, struct svgParseSymbol *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->ID) or (!Args->Viewport)) return log.warning(ERR::NullArgs);

   if (auto tagref = find_href_tag(Self, Args->ID)) {
      svgState state(Self);
      process_children(Self, state, *tagref, Args->Viewport);
      return ERR::Okay;
   }
   else {
      log.warning("Symbol '%s' not found.", Args->ID);
      return ERR::NotFound;
   }
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

static ERR SVG_Render(extSVG *Self, struct svgRender *Args)
{
   if (!Args) return ERR::NullArgs;

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

   return ERR::Okay;
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

static ERR SVG_SaveImage(extSVG *Self, struct acSaveImage *Args)
{
   ERR error;

   if (!Args) return ERR::NullArgs;

   LONG width = 0;
   LONG height = 0;
   Self->Scene->get(FID_PageWidth, &width);
   Self->Scene->get(FID_PageHeight, &height);

   if (!width) width = 1920;
   if (!height) height = 1080;

   auto pic = objPicture::create { fl::Width(width), fl::Height(height), fl::Flags(PCF::ALPHA|PCF::NEW) };
   if (pic.ok()) {
      if ((error = svgRender(Self, pic->Bitmap, 0, 0, width, height)) IS ERR::Okay) {
         if ((error = acSaveImage(*pic, Args->Dest, Args->ClassID)) IS ERR::Okay) {
            return ERR::Okay;
         }
      }
   }
   else error = ERR::CreateObject;

   return error;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Saves the SVG document to a data object.
-END-
*********************************************************************************************************************/

static ERR SVG_SaveToObject(extSVG *Self, struct acSaveToObject *Args)
{
   pf::Log log;
   static const char header[] =
"<?xml version=\"1.0\" standalone=\"no\"?>\n\
<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
   ERR (**actions)(OBJECTPTR, APTR);

   if (!Self->Viewport) return log.warning(ERR::NoData);

   if ((Args->ClassID) and (Args->ClassID != ID_SVG)) {
      auto mc = (objMetaClass *)FindClass(Args->ClassID);
      if ((mc->getPtr(FID_ActionTable, &actions) IS ERR::Okay) and (actions)) {
         if ((actions[AC_SaveToObject]) and (actions[AC_SaveToObject] != (APTR)SVG_SaveToObject)) {
            return actions[AC_SaveToObject](Self, Args);
         }
         else if ((actions[AC_SaveImage]) and (actions[AC_SaveImage] != (APTR)SVG_SaveImage)) {
            struct acSaveImage saveimage = { .Dest = Args->Dest };
            return actions[AC_SaveImage](Self, &saveimage);
         }
         else return log.warning(ERR::NoSupport);
      }
      else return log.warning(ERR::GetField);
   }
   else {
      auto xml = objXML::create { fl::Flags(XMF::NEW|XMF::READABLE) };

      if (xml.ok()) {
         Self->XML = *xml;

         ERR error = xmlInsertXML(*xml, 0, XMI::NIL, header, NULL);
         LONG index = xml->Tags.back().ID;

         XMLTag *tag;
         if ((error = xmlInsertStatement(*xml, index, XMI::NEXT, "<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:parasol=\"http://www.parasol.ws/xmlns/svg\"/>", &tag)) IS ERR::Okay) {
            bool multiple_viewports = (Self->Scene->Viewport->Next) ? true : false;
            if (multiple_viewports) {
               if ((error = save_svg_defs(Self, *xml, Self->Scene, index)) IS ERR::Okay) {
                  for (auto scan=Self->Scene->Viewport; scan; scan=(objVectorViewport *)scan->Next) {
                     if (!scan->Child) continue; // Ignore dummy viewports with no content
                     save_svg_scan(Self, *xml, scan, index);
                  }

                  error = xml->saveToObject(Args->Dest);
               }
            }
            else {
               DOUBLE x, y, width, height;

               if (error IS ERR::Okay) error = Self->Viewport->get(FID_ViewX, &x);
               if (error IS ERR::Okay) error = Self->Viewport->get(FID_ViewY, &y);
               if (error IS ERR::Okay) error = Self->Viewport->get(FID_ViewWidth, &width);
               if (error IS ERR::Okay) error = Self->Viewport->get(FID_ViewHeight, &height);

               if (error IS ERR::Okay) {
                  char buffer[80];
                  snprintf(buffer, sizeof(buffer), "%g %g %g %g", x, y, width, height);
                  xmlNewAttrib(tag, "viewBox", buffer);
               }

               LONG dim;
               if ((error IS ERR::Okay) and ((error = Self->Viewport->get(FID_Dimensions, &dim)) IS ERR::Okay)) {
                  if ((dim & (DMF_SCALED_X|DMF_FIXED_X)) and (Self->Viewport->get(FID_X, &x) IS ERR::Okay))
                     set_dimension(tag, "x", x, dim & DMF_SCALED_X);

                  if ((dim & (DMF_SCALED_Y|DMF_FIXED_Y)) and (Self->Viewport->get(FID_Y, &y) IS ERR::Okay))
                     set_dimension(tag, "y", y, dim & DMF_SCALED_Y);

                  if ((dim & (DMF_SCALED_WIDTH|DMF_FIXED_WIDTH)) and (Self->Viewport->get(FID_Width, &width) IS ERR::Okay))
                     set_dimension(tag, "width", width, dim & DMF_SCALED_WIDTH);

                  if ((dim & (DMF_SCALED_HEIGHT|DMF_FIXED_HEIGHT)) and (Self->Viewport->get(FID_Height, &height) IS ERR::Okay))
                     set_dimension(tag, "height", height, dim & DMF_SCALED_HEIGHT);
               }

               if (error IS ERR::Okay) {
                  if ((error = save_svg_defs(Self, *xml, Self->Scene, index)) IS ERR::Okay) {
                     for (auto scan=((objVector *)Self->Viewport)->Child; scan; scan=scan->Next) {
                        save_svg_scan(Self, *xml, scan, index);
                     }

                     error = xml->saveToObject(Args->Dest);
                  }
               }
            }

            Self->XML = NULL;
         }

         return error;
      }
      else return ERR::CreateObject;
   }
}

/*********************************************************************************************************************

-FIELD-
Colour: Defines the default fill to use for 'currentColor' references.

Set the Colour value to alter the default fill that is used for `currentColor` references.  Typically a standard RGB
painter fill reference should be used for this purpose, e.g. `rgb(255,255,255)`.  It is however, also acceptable to use 
URL references to named definitions such as gradients and images.  This will work as long as the named definition is
registered in the top-level @VectorScene object.

*********************************************************************************************************************/

static ERR GET_Colour(extSVG *Self, CSTRING *Value)
{
   *Value = Self->Colour.c_str();
   return ERR::Okay;
}

static ERR SET_Colour(extSVG *Self, CSTRING Value)
{
   if ((Value) and (*Value)) {
      Self->Colour.assign(Value);
   }
   return ERR::Okay;
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

static ERR GET_FrameCallback(extSVG *Self, FUNCTION **Value)
{
   if (Self->FrameCallback.defined()) {
      *Value = &Self->FrameCallback;
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

static ERR SET_FrameCallback(extSVG *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->FrameCallback.isScript()) UnsubscribeAction(Self->FrameCallback.Context, AC_Free);
      Self->FrameCallback = *Value;
      if (Self->FrameCallback.isScript()) {
         SubscribeAction(Self->FrameCallback.Context, AC_Free, C_FUNCTION(notify_free_frame_callback));
      }
   }
   else Self->FrameCallback.clear();
   return ERR::Okay;
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

static ERR SET_FrameRate(extSVG *Self, LONG Value)
{
   if ((Value >= 20) and (Value <= 1000)) {
      Self->FrameRate = Value;
      return ERR::Okay;
   }
   else return ERR::OutOfRange;
}

/*********************************************************************************************************************

-FIELD-
Path: A path referring to an SVG file.

SVG data can be loaded from a file source by setting the Path field to an SVG file.

*********************************************************************************************************************/

static ERR GET_Path(extSVG *Self, STRING *Value)
{
   *Value = Self->Path;
   return ERR::Okay;
}

static ERR SET_Path(extSVG *Self, CSTRING Value)
{
   if (Self->Path)   { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Folder) { FreeResource(Self->Folder); Self->Folder = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->Path = StrClone(Value))) return ERR::AllocMemory;
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Scene: The VectorScene that manages the Target object is referenced here.

The Scene is a read-only field that assists in quickly finding the @VectorScene that owns the #Target object.

*********************************************************************************************************************/

static ERR GET_Scene(extSVG *Self, objVectorScene **Value)
{
   *Value = Self->Scene;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Statement: A string containing SVG data.

SVG data can be loaded from a string by specifying it here prior to initialisation.  If the #Path field has been
defined, it will take precedent and the Statement is ignored.

Alternatively the #DataFeed() action can be used to parse data on-the-fly after the SVG object is initialised.

*********************************************************************************************************************/

static ERR SET_Statement(extSVG *Self, CSTRING Value)
{
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }

   if ((Value) and (*Value)) {
      if (!(Self->Statement = StrClone(Value))) return ERR::AllocMemory;
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Target: The container object for new SVG content can be declared here.

During the normal initialisation process, a new @VectorViewport is created to host the SVG scene graph.  By default,
the viewport and its content is strictly owned by the SVG object unless a Target is defined to redirect the scene
graph elsewhere.

The provided Target can be any object class, as long as it forms part of a scene graph owned by a @VectorScene
object.  It is recommended that the chosen target is a @VectorViewport.

The use of a Target will make the generated scene graph independent of the SVG object.  Consequently, it is possible
to terminate the SVG object without impacting the resources it created.

*********************************************************************************************************************/

static ERR SET_Target(extSVG *Self, OBJECTPTR Value)
{
   if (Value->Class->ClassID IS ID_VECTORSCENE) {
      Self->Target = Value;
      Self->Scene = (objVectorScene *)Value;
      if (Self->Scene->Viewport) Self->Viewport = Self->Scene->Viewport;
   }
   else {
      auto owner = Value->Owner;
      while ((owner) and (owner->Class->ClassID != ID_VECTORSCENE)) {
         owner = owner->Owner;
      }

      if (owner) {
         Self->Scene = (objVectorScene *)owner;
         Self->Target = Value;
         if (Self->Scene->Viewport) Self->Viewport = Self->Scene->Viewport;
      }
      else return ERR::Failed;
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Title: The title of the SVG document.

The title of an SVG document is declared with a title element that can embedded anywhere in the document.  In cases
where a title has been specified, it will be possible to read it from this field.  If no title is in the document then
NULL will be returned.

*********************************************************************************************************************/

static ERR SET_Title(extSVG *Self, CSTRING Value)
{
   if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
   if (Value) Self->Title = StrClone(Value);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Viewport: Returns the first viewport created by an SVG document.

This field simplifies the process of finding the first @VectorViewport that was created by a loaded SVG document.  NULL
is returned if an SVG document has not been successfully parsed yet.
-END-

*********************************************************************************************************************/

static ERR GET_Viewport(extSVG *Self, OBJECTPTR *Value)
{
   if (!Self->initialised()) return ERR::NotInitialised;
   *Value = Self->Viewport;
   return ERR::Okay;
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
   { "Statement", FDF_STRING|FDF_RW, NULL, SET_Statement },
   { "Frame",     FDF_LONG|FDF_RW, NULL, NULL },
   { "Flags",     FDF_LONGFLAGS|FDF_RW, NULL, NULL, &clSVGFlags },
   { "FrameRate", FDF_LONG|FDF_RW, NULL, SET_FrameRate },
   // Virtual Fields
   { "Colour",        FDF_VIRTUAL|FDF_STRING|FDF_RW, GET_Colour, SET_Colour },
   { "FrameCallback", FDF_VIRTUAL|FDF_FUNCTION|FDF_RW, GET_FrameCallback, SET_FrameCallback },
   { "Src",           FDF_VIRTUAL|FDF_SYNONYM|FDF_STRING|FDF_RW, GET_Path, SET_Path },
   { "Scene",         FDF_VIRTUAL|FDF_OBJECT|FDF_R, GET_Scene, NULL },
   { "Viewport",      FDF_VIRTUAL|FDF_OBJECT|FDF_R, GET_Viewport, NULL },
   END_FIELD
};

static ERR init_svg(void)
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

   return clSVG ? ERR::Okay : ERR::AddClass;
}
