/*****************************************************************************

-CLASS-
VectorColour: This is a stub class for use with the Vector module's DrawPath() function.

VectorColour is a stub class for exclusive use with the Vector module's DrawPath() function, for use in either the
StrokeStyle or FillStyle parameters.  VectorColour allows the path to be drawn with a solid colour, as specified in the
Red, Green, Blue and Alpha fields.

-FIELD-
Red: The red component value.

The red component value, measured from 0 to 1.0.  The default is 0.0.

-FIELD-
Green: The green component value.

The green component value, measured from 0 to 1.0.  The default is 0.0.

-FIELD-
Blue: The blue component value.

The blue component value, measured from 0 to 1.0.  The default is 0.0.

-FIELD-
Alpha: The alpha component value.

The alpha component value, measured from 0 to 1.0.  The default is 1.0.
-END-

*****************************************************************************/

static ERROR COLOUR_NewObject(objVectorColour *Self, APTR Void)
{
   Self->Alpha = 1.0;
   return ERR_Okay;
}

static const ActionArray clColourActions[] = {
   { AC_NewObject, (APTR)COLOUR_NewObject },
   { 0, NULL }
};

static const FieldArray clColourFields[] = {
   { "Red",   FDF_DOUBLE|FDF_RW, 0, NULL, NULL },
   { "Green", FDF_DOUBLE|FDF_RW, 0, NULL, NULL },
   { "Blue",  FDF_DOUBLE|FDF_RW, 0, NULL, NULL },
   { "Alpha", FDF_DOUBLE|FDF_RW, 0, NULL, NULL },
   END_FIELD
};

//****************************************************************************

static ERROR init_colour(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorColour,
      FID_BaseClassID|TLONG, ID_VECTORCOLOUR,
      FID_Name|TSTRING,      "VectorColour",
      FID_Category|TLONG,    CCF_GRAPHICS,
      FID_Actions|TPTR,      clColourActions,
      FID_Fields|TARRAY,     clColourFields,
      FID_Size|TLONG,        sizeof(objVectorColour),
      FID_Path|TSTR,         "modules:vector",
      TAGEND));
}

