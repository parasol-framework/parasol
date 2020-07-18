/*****************************************************************************

-CLASS-
SVG: Provides support for parsing and rendering SVG files.

The SVG class provides support for parsing SVG statements into native @Vector objects and related definitions.
For low-level vector programming, consider using the @Vector class directly, or use the SVG class to parse an
SVG script and then access the #Viewport field to perform transforms and manipulation of the vector group.

Please refer to the W3C documentation on SVG for a complete reference to the attributes that can be applied to SVG
elements.  Unfortunately we do not support all SVG capabilities at this time, but support will improve in future.

*****************************************************************************/

/*****************************************************************************
-ACTION-
Activate: Initiates playback of SVG animations.
-END-
*****************************************************************************/

static ERROR SVG_Activate(objSVG *Self, APTR Void)
{
   if (Self->Animated) {
      if (!Self->AnimationTimer) {
         FUNCTION timer;
         SET_FUNCTION_STDC(timer, animation_timer);
         SubscribeTimer(1.0 / (DOUBLE)Self->FrameRate, &timer, &Self->AnimationTimer);
      }
      else UpdateTimer(Self->AnimationTimer, 1.0 / (DOUBLE)Self->FrameRate);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Deactivate: Stops all playback of SVG animations.
-END-
*****************************************************************************/

static ERROR SVG_Deactivate(objSVG *Self, APTR Void)
{
   if (Self->AnimationTimer) { UpdateTimer(Self->AnimationTimer, 0); Self->AnimationTimer = 0; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
DataFeed: Vector graphics are created by passing XML-based instructions here.
-END-
*****************************************************************************/

static ERROR SVG_DataFeed(objSVG *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_XML) {
      return load_svg(Self, NULL, Args->Buffer);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR SVG_Free(objSVG *Self, APTR Void)
{
   if (Self->AnimationTimer) {
      UpdateTimer(Self->AnimationTimer, 0);
      Self->AnimationTimer = 0;
   }

   if ((Self->Target) AND (Self->Target IS &Self->Scene->Head) AND (Self->Scene->Head.OwnerID IS Self->Head.UniqueID)) {
      acFree(Self->Target);
      Self->Target = NULL;
   }

   if (Self->Path)  { FreeResource(Self->Path);  Self->Path = NULL; }
   if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }

   struct svgAnimation *anim = Self->Animations;
   while (anim) {
      struct svgAnimation *next = anim->Next;
      LONG i;
      for (i=0; i < anim->ValueCount; i++) {
         FreeResource(anim->Values[i]);
         anim->Values[i] = NULL;
      }
      FreeResource(anim);
      anim = next;
   }
   Self->Animations = NULL;

   svgID *symbol = Self->IDs;
   while (symbol) {
      svgID *next = symbol->Next;
      if (symbol->ID) { FreeResource(symbol->ID); symbol->ID = NULL; }
      FreeResource(symbol);
      symbol = next;
   }
   Self->IDs = NULL;

   svgInherit *inherit = Self->Inherit;
   while (inherit) {
      svgInherit *next = inherit->Next;
      FreeResource(inherit);
      inherit = next;
   }
   Self->Inherit = NULL;

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Init: Initialise the SVG object.

Initialising an SVG object will load an SVG source file if a #Path has been specified.  The default behaviour is to
generate the content in a local #Scene object, or alternatively the content can be redirected to an external
@VectorScene referred to by #Target.

-END-
*****************************************************************************/

static ERROR SVG_Init(objSVG *Self, APTR Void)
{
   if (!Self->Target) {
      if (!CreateObject(ID_VECTORSCENE, NF_INTEGRAL, &Self->Target, TAGEND)) {
         Self->Scene = (objVectorScene *)Self->Target;
      }
      else return ERR_NewObject;
   }

   if (Self->Path) return load_svg(Self, Self->Path, NULL);

   return ERR_Okay;
}

//****************************************************************************

static ERROR SVG_NewObject(objSVG *Self, APTR Void)
{
   #ifdef __ANDROID__
      Self->FrameRate = 30; // Choose a lower frame rate for Android devices, so as to minimise power consumption.
   #else
      Self->FrameRate = 60;
   #endif
   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR SVG_Render(objSVG *Self, struct svgRender *Args)
{
   if (!Args) return ERR_NullArgs;

   objBitmap *bmp = Args->Bitmap;
   LONG page_width = Args->Width;
   LONG page_height = Args->Height;

   SetPointer(Self->Scene, FID_Bitmap, bmp);

   SetLong(Self->Scene, FID_PageWidth, page_width);
   SetLong(Self->Scene, FID_PageHeight, page_height);

//   SetLong(Self->Scene->Viewport, FID_ViewX, Args->X);
//   SetLong(Self->Scene->Viewport, FID_ViewY, Args->Y);

   bmp->XOffset += Args->X;
   bmp->YOffset += Args->Y;

   Action(AC_Draw, Self->Scene, NULL);

   bmp->XOffset -= Args->X;
   bmp->YOffset -= Args->Y;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Frame: Forces the graphics to be drawn to a specific frame.

If this field is set to a valid frame number, the vector graphics will only be drawn when the frame of the container
matches the Frame number in this field.  When set to 0 (the default), the Vector will be drawn regardless of the
container's frame number.

-FIELD-
FrameCallback: Optional callback that is triggered whenever a new frame is prepared.

If an SVG object is being animated, ...

*****************************************************************************/

static ERROR GET_FrameCallback(objSVG *Self, FUNCTION **Value)
{
   if (Self->FrameCallback.Type != CALL_NONE) {
      *Value = &Self->FrameCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_FrameCallback(objSVG *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->FrameCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->FrameCallback.Script.Script, AC_Free);
      Self->FrameCallback = *Value;
      if (Self->FrameCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->FrameCallback.Script.Script, AC_Free);
   }
   else Self->FrameCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
FrameRate: The maximum frame rate to use when animating a vector scene.

This field defines the maximum frame rate that will be used when a vector scene is animated.  It is recommended that
a value between 50 and 100 is used.  It is important to note that while higher frame rates produce smoother animations,
they also increase the CPU usage proportionately.  For instance, a frame rate of 100 will use the CPU twice as much as
a frame rate of 50.  This will subsequently have an effect on power consumption.

The recommended frame rate is 60, as this will match the majority of modern displays.

*****************************************************************************/

static ERROR SET_FrameRate(objSVG *Self, LONG Value)
{
   if ((Value >= 20) AND (Value <= 1000)) {
      Self->FrameRate = Value;
      return ERR_Okay;
   }
   else return PostError(ERR_OutOfRange);
}

/*****************************************************************************

-FIELD-
Path: The location of the source SVG data.

SVG data can be loaded from a file source by setting the Path field to an SVG file.

*****************************************************************************/

static ERROR GET_Path(objSVG *Self, STRING *Value)
{
   *Value = Self->Path;
   return ERR_Okay;
}

static ERROR SET_Path(objSVG *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if ((Value) AND (*Value)) {
      if (!(Self->Path = StrClone(Value))) return PostError(ERR_AllocMemory);
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Scene: The VectorScene that manages the Target object is referenced here.

The Scene is a read-only field that assists in quickly finding the @VectorScene that owns the #Target object.

*****************************************************************************/

static ERROR GET_Scene(objSVG *Self, objVectorScene **Value)
{
   *Value = Self->Scene;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Target: The root Viewport that is generated during SVG initialisation can be created as a child of this target object.

During the initialisation of an SVG object, a VectorViewport will be created that hosts the SVG's vector objects.  The
target of this VectorViewport can be specified here, conditional on that object residing within a @VectorScene, or is a
VectorScene itself.

An attempt will be made to find the new target's parent VectorScene.  If none is identified, an error will be returned
and no further action is taken.

If a SVG object is initialised with no Target being defined, a @VectorScene will be created automatically and
referenced by the Target field.

*****************************************************************************/

static ERROR SET_Target(objSVG *Self, OBJECTPTR Value)
{
   if (Value->ClassID IS ID_VECTORSCENE) {
      Self->Target = Value;
      Self->Scene = (objVectorScene *)Value;
   }
   else {
      OBJECTID owner_id = GetOwner(Value);
      while ((owner_id) AND (GetClassID(owner_id) != ID_VECTORSCENE)) {
         owner_id = GetOwnerID(owner_id);
      }

      if (!owner_id) return PostError(ERR_Failed);

      Self->Scene = (objVectorScene *)GetObjectPtr(owner_id);
      Self->Target = Value;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Title: The title of the SVG document.

The title of an SVG document is declared with a title element that can embedded anywhere in the document.  In cases
where a title has been specified, it will be possible to read it from this field.  If no title is in the document then
NULL will be returned.

*****************************************************************************/

static ERROR SET_Title(objSVG *Self, CSTRING Value)
{
   if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
   if (Value) Self->Title = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Viewport: Returns the first viewport created by an SVG document.

This field simplifies the process of finding the first @VectorViewport that was created by a loaded SVG document.  NULL
is returned if an SVG document has not been successfully parsed yet.
-END-

*****************************************************************************/

static ERROR GET_Viewport(objSVG *Self, OBJECTPTR *Value)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) return ERR_NotInitialised;
   *Value = Self->Viewport;
   return ERR_Okay;
}

//****************************************************************************

#include "animation.c"
#include "gradients.c"
#include "parser.c"

//****************************************************************************

#include "class_svg_def.c"

static const struct FieldArray clSVGFields[] = {
   { "Target",    FDF_OBJECT|FDF_RI,    0, NULL, SET_Target },
   { "Path",      FDF_STRING|FDF_RW,    0, GET_Path, SET_Path },
   { "Title",     FDF_STRING|FDF_RW,    0, NULL, SET_Title },
   { "Frame",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Flags",     FDF_LONGFLAGS|FDF_RW, (MAXINT)&clSVGFlags, NULL, NULL },
   { "FrameRate", FDF_LONG|FDF_RW,      0, NULL, SET_FrameRate },
   { "FrameCallback", FDF_FUNCTION|FDF_RW, 0, GET_FrameCallback, SET_FrameCallback },
   // Virtual Fields
   { "Src",      FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, GET_Path, SET_Path },
   { "Scene",    FDF_VIRTUAL|FDF_OBJECT|FDF_R, 0, GET_Scene, NULL },
   { "Viewport", FDF_VIRTUAL|FDF_OBJECT|FDF_R, 0, GET_Viewport, NULL },
   END_FIELD
};

static ERROR init_svg(void) {
   return CreateObject(ID_METACLASS, 0, &clSVG,
      FID_ClassVersion|TFLOAT, VER_SVG,
      FID_Name|TSTR,      "SVG",
      FID_Category|TLONG, CCF_GUI,
      FID_Actions|TPTR,   clSVGActions,
      FID_Methods|TARRAY, clSVGMethods,
      FID_Fields|TARRAY,  clSVGFields,
      FID_Flags|TLONG,    CLF_PRIVATE_ONLY|CLF_PROMOTE_INTEGRAL,
      FID_Size|TLONG,     sizeof(objSVG),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}
