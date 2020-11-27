/*****************************************************************************

-CLASS-
VectorImage: Provides support for the filling and stroking of vectors with bitmap images.

The VectorImage class is used by Vector painting algorithms to fill and stroke vectors with bitmap images.  This is
achieved by initialising a VectorImage object with the desired settings and then registering it with
a @VectorScene via the @VectorScene.AddDef() method.

Any vector within the target scene will be able to utilise the image for filling or stroking by referencing its
name through the @Vector.Fill and @Vector.Stroke fields.  For instance 'url(#logo)'.

It is strongly recommended that the VectorImage is owned by the @VectorScene that is handling the
definition.  This will ensure that the VectorImage is de-allocated when the scene is destroyed.
-END-

*****************************************************************************/

static ERROR IMAGE_Init(objVectorImage *Self, APTR Void)
{
   parasol::Log log;

   if ((Self->SpreadMethod <= 0) OR (Self->SpreadMethod >= VSPREAD_END)) {
      log.traceWarning("Invalid SpreadMethod value of %d", Self->SpreadMethod);
      return log.warning(ERR_OutOfRange);
   }

   if ((Self->Units != VUNIT_BOUNDING_BOX) AND (Self->Units != VUNIT_USERSPACE)) {
      log.traceWarning("Invalid Units value of %d", Self->Units);
      return log.warning(ERR_OutOfRange);
   }

   if (!Self->Bitmap) return log.warning(ERR_FieldNotSet);

   if ((Self->Bitmap->BitsPerPixel != 24) AND (Self->Bitmap->BitsPerPixel != 32)) {
      return log.warning(ERR_NoSupport);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR IMAGE_NewObject(objVectorImage *Self, APTR Void)
{
   Self->Units = VUNIT_BOUNDING_BOX;
   Self->SpreadMethod = VSPREAD_PAD;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bitmap: Reference to a source bitmap for the rendering algorithm.

This field must be set prior to initialisation.  It will refer to a source bitmap that will be used by the rendering
algorithm.

*****************************************************************************/

static ERROR IMAGE_SET_Bitmap(objVectorImage *Self, objBitmap *Value)
{
   Self->Bitmap = Value;
   Self->Picture = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

Of the Dimension flags that are available, only FIXED_X, FIXED_Y, RELATIVE_X and RELATIVE_Y are applicable.

-FIELD-
Picture: Refers to a @Picture from which the source #Bitmap is acquired.

If an image bitmap is sourced from a @Picture then this field may be used to refer to the @Picture object.  The picture
will not be used directly by the VectorImage, as only the bitmap is of interest.

*****************************************************************************/

static ERROR IMAGE_SET_Picture(objVectorImage *Self, objPicture *Value)
{
   Self->Picture = Value;
   if (Value) Self->Bitmap = Value->Bitmap;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SpreadMethod: Defines the drawing mode.

The SpreadMethod defines the way in which the image is drawn within the target area.  The default setting is PAD.

-FIELD-
Units: Declares the coordinate system to use for the #X and #Y values.

This field declares the coordinate system that is used for values in the #X and #Y fields.  The default is BOUNDING_BOX.

-FIELD-
X: Apply a horizontal offset to the image, the origin of which is determined by the #Units value.

-FIELD-
Y: Apply a vertical offset to the image, the origin of which is determined by the #Units value.
-END-

*****************************************************************************/

static const ActionArray clImageActions[] = {
   { AC_Init,      (APTR)IMAGE_Init },
   { AC_NewObject, (APTR)IMAGE_NewObject },
   { 0, NULL }
};

static const FieldDef clImageSpread[] = {
   { "Pad",      VSPREAD_PAD },
   { "Repeat",   VSPREAD_REPEAT },
   { "ReflectX", VSPREAD_REFLECT_X },
   { "ReflectY", VSPREAD_REFLECT_Y },
   { "Clip",     VSPREAD_CLIP },
   { NULL, 0 }
};

static const FieldDef clImageUnits[] = {
   { "BoundingBox", VUNIT_BOUNDING_BOX },  // Coordinates are relative to the object's bounding box
   { "UserSpace",   VUNIT_USERSPACE },    // Coordinates are relative to the current viewport
   { NULL, 0 }
};

static const FieldDef clImageDimensions[] = {
   { "RelativeX", DMF_RELATIVE_X },
   { "RelativeY", DMF_RELATIVE_Y },
   { NULL, 0 }
};

static const FieldArray clImageFields[] = {
   { "X",            FDF_DOUBLE|FDF_RW, 0, NULL, NULL },
   { "Y",            FDF_DOUBLE|FDF_RW, 0, NULL, NULL },
   { "Picture",      FDF_OBJECT|FDF_RW, ID_PICTURE, NULL, (APTR)IMAGE_SET_Picture },
   { "Bitmap",       FDF_OBJECT|FDF_RW, ID_BITMAP,  NULL, (APTR)IMAGE_SET_Bitmap },
   { "Units",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clImageUnits, NULL, NULL },
   { "Dimensions",   FDF_LONGFLAGS|FDF_RW,      (MAXINT)&clImageDimensions, NULL, NULL },
   { "SpreadMethod", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clImageSpread, NULL, NULL },
 //{ "Src",          FDF_STRING|FDF_W, 0, NULL, (APTR)IMAGE_SET_Src },
   END_FIELD
};

static ERROR init_image(void) // The gradient is a definition type for creating gradients and not drawing.
{
   return(CreateObject(ID_METACLASS, 0, &clVectorImage,
      FID_BaseClassID|TLONG, ID_VECTORIMAGE,
      FID_Name|TSTRING,      "VectorImage",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clImageActions,
      FID_Fields|TARRAY,     clImageFields,
      FID_Size|TLONG,        sizeof(objVectorImage),
      FID_Path|TSTR,         "modules:vector",
      TAGEND));
}

