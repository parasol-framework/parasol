/*****************************************************************************

-CLASS-
VectorGroup: Extends the Vector class with support for organising vectors into groups.

Groups have a singular purpose of aiding the structural definition when linking and grouping vector objects.  Groups
have a passive effect on the drawing process and can be effective at assigning inheritable attributes to child vectors.

-END-

*****************************************************************************/

static ERROR init_group(void)
{
   return(CreateObject(ID_METACLASS, 0, &clVectorGroup,
      FID_BaseClassID|TLONG,    ID_VECTOR,
      FID_SubClassID|TLONG,     ID_VECTORGROUP,
      FID_Name|TSTRING,         "VectorGroup",
      FID_Category|TLONG,       CCF_GRAPHICS,
      FID_Size|TLONG,           sizeof(objVector),
      FID_Path|TSTR,            MOD_PATH,
      TAGEND));
}
