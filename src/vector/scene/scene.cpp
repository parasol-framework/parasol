/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
VectorScene: Manages the scene graph for a collection of vectors.

The VectorScene class acts as a container and control point for the management of vector definitions.  Its primary
duty is to draw the scene to a target Bitmap provided by the client.

Vector scenes are created by initialising multiple Vector objects such as @VectorPath and
@VectorViewport and positioning them within a vector tree.  The VectorScene must lie at the root.

To draw a scene, the client must set the target #Bitmap and call the #Draw() action.

Vector definitions can be saved and loaded from permanent storage by using the @SVG class.
-END-

*****************************************************************************/

static ERROR VECTORSCENE_Reset(objVectorScene *, APTR);
static void render_to_surface(objVectorScene *, objSurface *, objBitmap *);

/*****************************************************************************

-METHOD-
AddDef: Adds a new definition to a vector tree.

This method will add a new definition to the root of a vector tree.  This feature is provided with the intention of
supporting SVG style references to definitions such as gradients, images and other vectors.  By providing a name with
the definition object, the object can then be referenced in strings that support definition referencing.

For instance, creating a gradient and associating it with the definition "redGradient" it would be possible to
reference it with the string "url(#redGradient)" from common graphics attributes such as "fill".

-INPUT-
cstr Name: The unique name to associate with the definition.
obj Def: Reference to the definition object.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR VECTORSCENE_AddDef(objVectorScene *Self, struct scAddDef *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name) or (!Args->Def)) return log.warning(ERR_NullArgs);

   OBJECTPTR def = (OBJECTPTR)Args->Def;

   if ((def->ClassID IS ID_VECTORSCENE) or
       (def->ClassID IS ID_VECTOR) or
       (def->ClassID IS ID_VECTORGRADIENT) or
       (def->ClassID IS ID_VECTORIMAGE) or
       (def->ClassID IS ID_VECTORPATH) or
       (def->ClassID IS ID_VECTORPATTERN) or
       (def->ClassID IS ID_VECTORFILTER) or
       (def->ClassID IS ID_VECTORTRANSITION) or
       (def->SubID IS ID_VECTORCLIP)) {
      // The use of this object as a definition is valid.
   }
   else return log.warning(ERR_InvalidObject);

   // If the resource does not belong to the Scene object, this can lead to invalid pointer references

   if (def->OwnerID != Self->Head.UniqueID) {
      log.warning("The %s must belong to VectorScene #%d, but is owned by object #%d.", def->Class->ClassName, Self->Head.UniqueID, def->OwnerID);
      return ERR_UnsupportedOwner;
   }

   // TO DO: Subscribe to the Free() action of the definition object so that we can avoid invalid pointer references.

   log.trace("Adding definition '%s' for object #%d", Args->Name, def->UniqueID);

   APTR data;
   if (!Self->Defs) {
      if (!(Self->Defs = VarNew(64, KSF_CASE))) {
         return log.warning(ERR_AllocMemory);
      }
   }
   else if (!VarGet(Self->Defs, Args->Name, &data, NULL)) { // Check that the definition name is unique.
      log.warning("The vector definition name '%s' is already in use.", Args->Name);
      return ERR_InvalidValue;
   }

   VectorDef vd;
   vd.Object = def;
   VarSet(Self->Defs, Args->Name, &vd, sizeof(vd));
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Draw: Renders the scene to a bitmap.

The Draw action will render the scene to the target #Bitmap.  If #Bitmap is NULL, an error will be
returned.

In addition, the #RenderTime field will be updated if the RENDER_TIME flag is defined.

-ERRORS-
Okay
FieldNotSet: The Bitmap field is NULL.

*****************************************************************************/

static ERROR VECTORSCENE_Draw(objVectorScene *Self, struct acDraw *Args)
{
   parasol::Log log;
   objBitmap *bmp;

   if (!(bmp = Self->Bitmap)) return log.warning(ERR_FieldNotSet);

   // Allocate the adaptor, or if the existing adaptor doesn't match the Bitmap pixel type, reallocate it.

   VMAdaptor *adaptor;

   const LONG type = (bmp->BitsPerPixel << 8) | (bmp->BytesPerPixel);
   if (type != Self->AdaptorType) {
      if (Self->Adaptor) {
         delete Self->Adaptor;
         Self->Adaptor = NULL;
      }

      adaptor = new (std::nothrow) VMAdaptor;
      if (!adaptor) return log.warning(ERR_AllocMemory);
      adaptor->Scene = Self;
      Self->Adaptor = adaptor;
      Self->AdaptorType = type;
   }
   else adaptor = static_cast<VMAdaptor *> (Self->Adaptor);

   if (Self->Flags & VPF_RENDER_TIME) {
      LARGE time = PreciseTime();
      adaptor->draw(bmp);
      if ((Self->RenderTime = PreciseTime() - time) < 1) Self->RenderTime = 1;
   }
   else adaptor->draw(bmp);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
FindDef: Search for a vector definition by name.

Use the FindDef method to search for a vector definition by name.  A reference to the definition will be returned if
the search is successful.

Definitions are created with the #AddDef() method.

-INPUT-
cstr Name: The name of the definition.
&obj Def: A pointer to the definition is returned here if discovered.

-ERRORS-
Okay
NullArgs
Search: A definition with the given Name was not found.
-END-

*****************************************************************************/

static ERROR VECTORSCENE_FindDef(objVectorScene *Self, struct scFindDef *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   CSTRING name = Args->Name;

   if (*name IS '#') name = name + 1;
   else if (!StrCompare("url(#", name, 5, 0)) {
      char newname[80];
      UWORD i;
      name += 5;
      for (i=0; (name[i] != ')') and (name[i]) and (i < sizeof(newname)-1); i++) newname[i] = name[i];
      newname[i] = 0;

      VectorDef *vd;
      if (!VarGet(Self->Defs, newname, &vd, NULL)) {
         Args->Def = vd->Object;
         return ERR_Okay;
      }
      else return ERR_Search;
   }

   VectorDef *vd;
   if (!VarGet(Self->Defs, name, &vd, NULL)) {
      Args->Def = vd->Object;
      return ERR_Okay;
   }
   else return ERR_Search;
}

//****************************************************************************

static ERROR VECTORSCENE_Free(objVectorScene *Self, APTR Args)
{
   if (Self->Viewport) Self->Viewport->Parent = NULL;
   if (Self->Adaptor) { delete Self->Adaptor; Self->Adaptor = NULL; }
   if (Self->Buffer) { delete Self->Buffer; Self->Buffer = NULL; }
   if (Self->Defs) { FreeResource(Self->Defs); Self->Defs = NULL; }
   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORSCENE_Init(objVectorScene *Self, APTR Void)
{
   // Setting the SurfaceID is optional and enables auto-rendering to the display.  The
   // alternative for the client is to set the Bitmap field and manage rendering manually.

   if (Self->SurfaceID) {
      OBJECTPTR surface;
      if ((Self->SurfaceID) and (!AccessObject(Self->SurfaceID, 5000, &surface))) {
         auto callback = make_function_stdc(render_to_surface);
         struct drwAddCallback args = { &callback };
         Action(MT_DrwAddCallback, surface, &args);
         ReleaseObject(surface);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR VECTORSCENE_NewObject(objVectorScene *Self, APTR Void)
{
   Self->SampleMethod = VSM_BILINEAR;
   // Please refer to the Reset action for setting variable defaults
   return VECTORSCENE_Reset(Self, NULL);
}

/*****************************************************************************
-ACTION-
Redimension: Redefines the size of the page.
-END-
*****************************************************************************/

static ERROR VECTORSCENE_Redimension(objVectorScene *Self, struct acRedimension *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Width >= 1.0)  Self->PageWidth  = F2T(Args->Width);
   if (Args->Height >= 1.0) Self->PageHeight = F2T(Args->Height);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Reset: Clears all registered definitions and resets field values.  Child vectors are untouched.
-END-
*****************************************************************************/

static ERROR VECTORSCENE_Reset(objVectorScene *Self, APTR Void)
{
   if (Self->Adaptor) { delete Self->Adaptor; Self->Adaptor = NULL; }
   if (Self->Buffer) { delete Self->Buffer; Self->Buffer = NULL; }
   if (Self->Defs) { FreeResource(Self->Defs); Self->Defs = NULL; }

   if (!(Self->Head.Flags & NF_FREE)) { // Reset all variables
      Self->Gamma = 1.0;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Resize: Redefines the size of the page.
-END-
*****************************************************************************/

static ERROR VECTORSCENE_Resize(objVectorScene *Self, struct acResize *Args)
{
   if (!Args) return ERR_NullArgs;
   if (Args->Width >= 1.0)  Self->PageWidth  = F2T(Args->Width);
   if (Args->Height >= 1.0) Self->PageHeight = F2T(Args->Height);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SearchByID: Search for a vector by numeric ID.

This method will search a scene for an object that matches a given ID (vector ID's can be set with the NumericID and ID
fields).  If multiple vectors are using the same ID, repeated calls can be made to this method to find all of them.
This is achieved by calling this method on the vector that was last returned as a result.

Please note that searching for string-based ID's is achieved by converting the string to a case-sensitive hash
with StrHash() and using that as the ID.

-INPUT-
int ID: The ID to search for.
&obj Result: This parameter will be updated with the discovered vector, or NULL if not found.

-ERRORS-
Okay
NullArgs
Search: A vector with a matching ID was not found.
-END-

*****************************************************************************/

static ERROR VECTORSCENE_SearchByID(objVectorScene *Self, struct scSearchByID *Args)
{
   if (!Args) return ERR_NullArgs;
   Args->Result = NULL;

   objVector *vector = Self->Viewport;
   while (vector) {
      //log.msg("Search","%.3d: %p <- #%d -> %p Child %p", vector->Index, vector->Prev, vector->Head.UniqueID, vector->Next, vector->Child);
cont:
      if (vector->NumericID IS Args->ID) {
         Args->Result = (OBJECTPTR)vector;
         return ERR_Okay;
      }

      if (vector->Child) vector = vector->Child;
      else if (vector->Next) vector = vector->Next;
      else {
         while ((vector = (objVector *)get_parent(vector))) { // Unwind back up the stack, looking for the first Parent with a Next field.
            if (vector->Head.ClassID != ID_VECTOR) return ERR_Search;
            if (vector->Next) {
               vector = vector->Next;
               goto cont;
            }
         }
         return ERR_Search;
      }
   }

   return ERR_Search;
}

/*****************************************************************************
-FIELD-
Bitmap: Target bitmap for drawing vectors.

The target bitmap to use when drawing the vectors must be specified here.

*****************************************************************************/

static ERROR SET_Bitmap(objVectorScene *Self, objBitmap *Value)
{
   if (Value) {
      if (Self->Buffer) delete Self->Buffer;

      Self->Buffer = new (std::nothrow) agg::rendering_buffer;
      if (Self->Buffer) {
         Self->Buffer->attach(Value->Data, Value->Width, Value->Height, Value->LineWidth);
         Self->Bitmap = Value;

         if (Self->Flags & VPF_BITMAP_SIZED) {
            Self->PageWidth = Value->Width;
            Self->PageHeight = Value->Height;
         }
      }
      else return ERR_Memory;
   }
   else Self->Buffer = NULL;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
Gamma: Private. Not currently implemented.

-FIELD-
PageHeight: The height of the page that contains the vector.

This value defines the pixel height of the page that contains the vector definition.  If the RESIZE #Flags
option is used then the viewport will be scaled to fit within the page.

****************************************************************************/

static ERROR SET_PageHeight(objVectorScene *Self, LONG Value)
{
   if (Value IS Self->PageHeight) return ERR_Okay;
   if (Value < 1) Self->PageHeight = 1;
   else Self->PageHeight = Value;
   if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
PageWidth: The width of the page that contains the vector.

This value defines the pixel width of the page that contains the vector definition.  If the RESIZE #Flags
option is used then the viewport will be scaled to fit within the page.

****************************************************************************/

static ERROR SET_PageWidth(objVectorScene *Self, LONG Value)
{
   if (Value IS Self->PageWidth) return ERR_Okay;

   if (Value < 1) Self->PageWidth = 1;
   else Self->PageWidth = Value;
   if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM);
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
RenderTime: Returns the rendering time of the last scene.

RenderTime returns the rendering time of the last scene that was drawn, measured in microseconds.  This value can also
be used to compute frames-per-second with `1000000 / RenderTime`.

The RENDER_TIME flag should also be set before fetching this value, as it is required to enable the timing feature.  If
RENDER_TIME is not set, it will be set automatically so that subsequent calls succeed correctly.

****************************************************************************/

static ERROR GET_RenderTime(objVectorScene *Self, LARGE *Value)
{
   Self->Flags |= VPF_RENDER_TIME;
   *Value = Self->RenderTime;
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
SampleMethod: The sampling method to use when interpolating images and patterns.

The SampleMethod controls the sampling algorithm that is used when images and patterns in the vector definition are affected
by rotate, skew and scale transforms.  The choice of method will have a significant impact on the speed and quality of
the images that are displayed in the rendered scene.  The recommended default is BILINEAR, which provides a
comparatively average result and execution speed.  The most advanced method is BLACKMAN8, which produces an excellent
level of quality at the cost of very poor execution speed.

-FIELD-
Surface: May refer to a Surface object for enabling automatic rendering.

Setting the Surface field will enable automatic rendering to a display surface.

*****************************************************************************/

static ERROR SET_Surface(objVectorScene *Self, OBJECTID Value)
{
   Self->SurfaceID = Value;
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
Viewport: References the first object in the scene, which must be a VectorViewport object.

The first object in the vector scene is referenced here.  It must belong to the @VectorViewport class, because it will
be used to define the size and location of the area rendered by the scene.

The Viewport field must not be set by the client.  The VectorViewport object will configure its ownership to
the VectorScene prior to initialisation.  The Viewport field value will then be set automatically when the
VectorViewport object is initialised.
-END-

*****************************************************************************/

static void render_to_surface(objVectorScene *Self, objSurface *Surface, objBitmap *Bitmap)
{
   Self->Bitmap = Bitmap;

   if ((Self->PageWidth != Surface->Width) or (Self->PageHeight != Surface->Height)) {
      Self->PageWidth = Surface->Width;
      Self->PageHeight = Surface->Height;
      if (Self->Viewport) mark_dirty(Self->Viewport, RC_BASE_PATH|RC_TRANSFORM); // Base-paths need to be recomputed if they use relative coordinates.
   }

   acDraw(Self);
}

#include "scene_def.c"

static const FieldArray clSceneFields[] = {
   { "RenderTime",   FDF_LARGE|FDF_R,            0, (APTR)GET_RenderTime, NULL },
   { "Gamma",        FDF_DOUBLE|FDF_RW,          0, NULL, NULL },
   { "Viewport",     FDF_OBJECT|FD_R,            ID_VECTORVIEWPORT, NULL, NULL },
   { "Bitmap",       FDF_OBJECT|FDF_RW,          ID_BITMAP, NULL, (APTR)SET_Bitmap },
   { "Defs",         FDF_STRUCT|FDF_PTR|FDF_SYSTEM|FDF_R, (MAXINT)"KeyStore", NULL, NULL },
   { "Surface",      FDF_OBJECTID|FDF_RW,        ID_SURFACE, NULL, (APTR)SET_Surface },
   { "Flags",        FDF_LONGFLAGS|FDF_RW,       (MAXINT)&clVectorSceneFlags, NULL, NULL },
   { "PageWidth",    FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_PageWidth },
   { "PageHeight",   FDF_LONG|FDF_RW,            0, NULL, (APTR)SET_PageHeight },
   { "SampleMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clVectorSceneSampleMethod, NULL, NULL },
   END_FIELD
};

static ERROR init_vectorscene(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorScene,
      FID_ClassVersion|TFLOAT, VER_VECTORSCENE,
      FID_Name|TSTR,      "VectorScene",
      FID_Category|TLONG, CCF_GRAPHICS,
      FID_Actions|TPTR,   clVectorSceneActions,
      FID_Methods|TARRAY, clVectorSceneMethods,
      FID_Fields|TARRAY,  clSceneFields,
      FID_Size|TLONG,     sizeof(objVectorScene),
      FID_Path|TSTR,      "modules:vector",
      TAGEND));
}
