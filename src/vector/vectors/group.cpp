/*********************************************************************************************************************

-CLASS-
VectorGroup: Extends the Vector class with support for organising vectors into groups.

Groups provide a simple way of grouping vector objects.  Groups have a passive effect on the drawing process and can be
effective at assigning inheritable attributes to child vectors.

If there is a need to adjust the container dimensions, use a @VectorViewport instead.

-END-

NOTE: Groups can export a boundary if they have child paths, in which case all the paths are accummulated to form a
single bounds area.  The calc_full_boundary() function can be used to do this.

*********************************************************************************************************************/

static ERR init_group(void)
{
   clVectorGroup = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::VECTOR),
      fl::ClassID(CLASSID::VECTORGROUP),
      fl::Name("VectorGroup"),
      fl::Category(CCF::GRAPHICS),
      fl::Size(sizeof(extVector)),
      fl::Path(MOD_PATH));

   return clVectorGroup ? ERR::Okay : ERR::AddClass;
}
