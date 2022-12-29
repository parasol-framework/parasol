/*****************************************************************************

-CLASS-
VectorGroup: Extends the Vector class with support for organising vectors into groups.

Groups provide a simple way of grouping vector objects.  Groups have a passive effect on the drawing process and can be
effective at assigning inheritable attributes to child vectors.

If there is a need to adjust the container dimensions, use a @VectorViewport instead.

-END-

*****************************************************************************/

static ERROR init_group(void)
{
   clVectorGroup = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::SubClassID(ID_VECTORGROUP),
      fl::Name("VectorGroup"),
      fl::Category(CCF_GRAPHICS),
      fl::Size(sizeof(extVector)),
      fl::Path(MOD_PATH));

   return clVectorGroup ? ERR_Okay : ERR_AddClass;
}
