/*********************************************************************************************************************

-CLASS-
VectorShape: Extends the Vector class with support for the Superformula algorithm.

The VectorShape class extends the Vector class with support for generating paths with the Superformula algorithm by
Johan Gielis.  This feature is not part of the SVG standard and therefore should not be used in cases where SVG
compliance is a strict requirement.

The Superformula is documented in detail at Wikipedia: http://en.wikipedia.org/wiki/Superformula

-END-

*********************************************************************************************************************/

#define DEFAULT_VERTICES (360 * 4)

class extVectorShape : public extVector {
   public:
   static constexpr CLASSID CLASS_ID = ID_VECTORSHAPE;
   static constexpr CSTRING CLASS_NAME = "VectorShape";
   using create = pf::Create<extVectorShape>;

   DOUBLE Radius;
   DOUBLE CX, CY;
   DOUBLE M, N1, N2, N3, A, B, Phi;
   LONG Vertices;
   LONG Dimensions;
   LONG Spiral;
   LONG Repeat;
   UBYTE Close;
   UBYTE Mod;
};

//********************************************************************************************************************

static void generate_supershape(extVectorShape *Vector)
{
   DOUBLE cx = Vector->CX, cy = Vector->CY;

   if (Vector->Dimensions & DMF_RELATIVE_CENTER_X) {
      if (Vector->ParentView->vpDimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH)) cx *= Vector->ParentView->vpFixedWidth;
      else if (Vector->ParentView->vpViewWidth > 0) cx *= Vector->ParentView->vpViewWidth;
      else cx *= Vector->Scene->PageWidth;
   }

   if (Vector->Dimensions & DMF_RELATIVE_CENTER_Y) {
      if (Vector->ParentView->vpDimensions & (DMF_FIXED_HEIGHT|DMF_RELATIVE_HEIGHT)) cy *= Vector->ParentView->vpFixedHeight;
      else if (Vector->ParentView->vpViewHeight > 0) cy *= Vector->ParentView->vpViewHeight;
      else cy *= Vector->Scene->PageHeight;
   }

   const DOUBLE scale = Vector->Radius;
   DOUBLE rescale = 0;
   DOUBLE tscale = Vector->Transform.scale();

   DOUBLE vertices = Vector->Vertices;
   if (vertices IS DEFAULT_VERTICES) {
      if (Vector->Spiral > 1) vertices *= 2;
   }

   const DOUBLE m  = Vector->M;
   const DOUBLE n1 = Vector->N1;
   const DOUBLE n2 = Vector->N2;
   const DOUBLE n3 = Vector->N3;
   DOUBLE phi_a;
   if (Vector->Spiral > 1) phi_a = (agg::pi * Vector->Phi * DOUBLE(Vector->Spiral)) / vertices;
   else phi_a = (agg::pi * Vector->Phi) / vertices;
   const DOUBLE a = 1.0 / Vector->A;
   const DOUBLE b = 1.0 / Vector->B;

   LONG lx = 0x7fffffff, ly = 0x7fffffff;
   for (DOUBLE i=0; i < vertices; i++) {
      const DOUBLE phi = phi_a * i;
      const DOUBLE t1 = pow(std::abs(a * cos(m * phi * 0.25)), n2);
      const DOUBLE t2 = pow(std::abs(b * sin(m * phi * 0.25)), n3);
      DOUBLE r  = 1.0 / pow(t1 + t2, 1.0/n1);

      // These additional transforms can help in building a greater library of shapes.

      switch(Vector->Mod) {
         case 1: r = exp(r); break;
         case 2: r = log(r); break;
         case 3: r = atan(r); break;
         case 4: r = exp(1.0 / r); break;
         case 5: r = 1+fastPow(cos(r), 2); break;
         case 6: r = fastPow(sin(r),2); break;
         case 7: r = 1+fastPow(sin(r), 2); break;
         case 8: r = fastPow(cos(r),2); break;
      }

      DOUBLE x = r * cos(phi);
      DOUBLE y = r * sin(phi);

      x *= scale * tscale;
      y *= scale * tscale;

      // Prevent sub-pixel vertices from being generated.

      if ((F2I(x) IS lx) and (F2I(y) IS ly)) continue;
      lx = F2I(x);
      ly = F2I(y);

      // If x or y is greater than the radius, we'll have to rescale the final result after the shape has been generated.

      if (x > rescale) rescale = x;
      else if (y > rescale) rescale = y;

      if (i == 0.0) Vector->BasePath.move_to(x, y); // Plot the vertex
      else Vector->BasePath.line_to(x, y);
   }

   if (Vector->Spiral > 1) {
      DOUBLE total = Vector->BasePath.total_vertices();
      for (DOUBLE i=0; i < total; i++) {
         DOUBLE x, y;
         Vector->BasePath.vertex(i, &x, &y);
         x = x * (i / total);
         y = y * (i / total);
         Vector->BasePath.modify_vertex(i, x, y);
      }
   }
   else if (Vector->Repeat > 1) {
      Vector->BasePath.close_polygon(); // Repeated paths are always closed.

      agg::path_storage clone(Vector->BasePath);

      for (LONG i=0; i < Vector->Repeat-1; i++) {
         agg::trans_affine transform;
         transform.scale(DOUBLE(i+1) / DOUBLE(Vector->Repeat));
         agg::conv_transform<agg::path_storage, agg::trans_affine> scaled_path(clone, transform);
         Vector->BasePath.concat_path(scaled_path);
      }
   }
   else if (Vector->Close) Vector->BasePath.close_polygon();

   agg::trans_affine transform;
   if (rescale != scale) transform.scale(scale / rescale);
   transform.translate(cx, cy);
   Vector->BasePath.transform(transform);

   bounding_rect_single(Vector->BasePath, 0, &Vector->BX1, &Vector->BY1, &Vector->BX2, &Vector->BY2);
}

//********************************************************************************************************************

static ERROR SUPER_NewObject(extVectorShape *Self, APTR Void)
{
   Self->Radius = 100;
   Self->N1 = 0.1;
   Self->N2 = 1.7;
   Self->N3 = 1.7;
   Self->M = 5;
   Self->A = 1;
   Self->B = 1;
   Self->Phi = 2;
   Self->Vertices = DEFAULT_VERTICES;
   Self->Close = TRUE;
   Self->GeneratePath = (void (*)(extVector *))&generate_supershape;
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
A: A parameter for the Superformula.

This field sets the Superformula's 'A' parameter value.

*********************************************************************************************************************/

static ERROR SUPER_GET_A(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->A;
   return ERR_Okay;
}

static ERROR SUPER_SET_A(extVectorShape *Self, DOUBLE Value)
{
   Self->A = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
B: A parameter for the Superformula.

This field sets the Superformula's 'B' parameter value.

*********************************************************************************************************************/

static ERROR SUPER_GET_B(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->B;
   return ERR_Okay;
}

static ERROR SUPER_SET_B(extVectorShape *Self, DOUBLE Value)
{
   Self->B = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterX: The center of the shape on the x-axis.  Expressed as a fixed or relative coordinate.

The horizontal center of the shape is defined here as either a fixed or relative value.

*********************************************************************************************************************/

static ERROR SUPER_GET_CenterX(extVectorShape *Self, Variable *Value)
{
   DOUBLE val = Self->CX;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_CENTER_X)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SUPER_SET_CenterX(extVectorShape *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_X) & (~DMF_FIXED_CENTER_X);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_X) & (~DMF_RELATIVE_CENTER_X);

   Self->CX = val;

   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterY: The center of the shape on the y-axis.  Expressed as a fixed or relative coordinate.

The vertical center of the shape is defined here as either a fixed or relative value.

*********************************************************************************************************************/

static ERROR SUPER_GET_CenterY(extVectorShape *Self, Variable *Value)
{
   DOUBLE val = Self->CY;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_CENTER_Y)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SUPER_SET_CenterY(extVectorShape *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_CENTER_Y) & (~DMF_FIXED_CENTER_Y);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_CENTER_Y) & (~DMF_RELATIVE_CENTER_Y);

   Self->CY = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Close: A parameter for the super shape algorithm.

If TRUE, the shape path will be closed between the beginning and end points.

*********************************************************************************************************************/

static ERROR SUPER_GET_Close(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Close;
   return ERR_Okay;
}

static ERROR SUPER_SET_Close(extVectorShape *Self, LONG Value)
{
   Self->Close = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or relative values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_CENTER_X">The #CenterX value is a fixed coordinate.</>
<type name="FIXED_CENTER_Y">The #CenterY value is a fixed coordinate.</>
<type name="FIXED_RADIUS">The #Radius value is a fixed coordinate.</>
<type name="RELATIVE_CENTER_X">The #CenterX value is a relative coordinate.</>
<type name="RELATIVE_CENTER_Y">The #CenterY value is a relative coordinate.</>
<type name="RELATIVE_RADIUS">The #Radius value is a relative coordinate.</>
</types>

*********************************************************************************************************************/

static ERROR SUPER_GET_Dimensions(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Dimensions;
   return ERR_Okay;
}

static ERROR SUPER_SET_Dimensions(extVectorShape *Self, LONG Value)
{
   Self->Dimensions = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
M: A parameter for the Superformula.

This field sets the Superformula's 'M' parameter value.

*********************************************************************************************************************/

static ERROR SUPER_GET_M(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->M;
   return ERR_Okay;
}

static ERROR SUPER_SET_M(extVectorShape *Self, DOUBLE Value)
{
   Self->M = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Mod: A special modification parameter that alters the super shape algorithm.

The Mod field alters the super shape algorithm, sometimes in radical ways that allow entirely new shapes to be
discovered in the super shape library.  The value that is specified will result in a formula being applied to the
generated 'r' value.  Possible values and their effects are:

<types>
<type name="0">Default</>
<type name="1">exp(r)</>
<type name="2">log(r)</>
<type name="3">atan(r)</>
<type name="4">exp(1.0/r)</>
<type name="5">1+cos(r)^2</>
<type name="6">sin(r)^2</>
<type name="7">1+sin(r)^2</>
<type name="8">cos(r)^2</>
</types>

*********************************************************************************************************************/

static ERROR SUPER_GET_Mod(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Mod;
   return ERR_Okay;
}

static ERROR SUPER_SET_Mod(extVectorShape *Self, LONG Value)
{
   Self->Mod = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
N1: A parameter for the super shape algorithm.

This field sets the Superformula's 'N1' parameter value.

*********************************************************************************************************************/

static ERROR SUPER_GET_N1(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->N1;
   return ERR_Okay;
}

static ERROR SUPER_SET_N1(extVectorShape *Self, DOUBLE Value)
{
   Self->N1 = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
N2: A parameter for the super shape algorithm.

This field sets the Superformula's 'N2' parameter value.

*********************************************************************************************************************/

static ERROR SUPER_GET_N2(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->N2;
   return ERR_Okay;
}

static ERROR SUPER_SET_N2(extVectorShape *Self, DOUBLE Value)
{
   Self->N2 = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
N3: A parameter for the super shape algorithm.

This field sets the Superformula's 'N3' parameter value.

*********************************************************************************************************************/

static ERROR SUPER_GET_N3(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->N3;
   return ERR_Okay;
}

static ERROR SUPER_SET_N3(extVectorShape *Self, DOUBLE Value)
{
   Self->N3 = Value;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Phi: A parameter for the super shape algorithm.

The Phi value has an impact on the length of the generated path.  If the super shape parameters form a circular path
(whereby the last vertex meets the first) then the Phi value should not be modified.  If the path does not meet
itself then the Phi value should be increased until it does.  The minimum (and default) value is 2.  It is recommended
that the Phi value is increased in increments of 2 until the desired effect is achieved.

*********************************************************************************************************************/

static ERROR SUPER_GET_Phi(extVectorShape *Self, DOUBLE *Value)
{
   *Value = Self->Phi;
   return ERR_Okay;
}

static ERROR SUPER_SET_Phi(extVectorShape *Self, DOUBLE Value)
{
   if (Value >= 2.0) {
      Self->Phi = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Radius: The radius of the generated shape.  Expressed as a fixed or relative coordinate.

The Radius defines the final size of the generated shape.  It can be expressed in fixed or relative terms.

*********************************************************************************************************************/

static ERROR SUPER_GET_Radius(extVectorShape *Self, Variable *Value)
{
   DOUBLE val = Self->Radius;
   if ((Value->Type & FD_PERCENTAGE) and (Self->Dimensions & DMF_RELATIVE_RADIUS)) val = val * 100.0;
   if (Value->Type & FD_DOUBLE) Value->Double = val;
   else if (Value->Type & FD_LARGE) Value->Large = F2T(val);
   return ERR_Okay;
}

static ERROR SUPER_SET_Radius(extVectorShape *Self, Variable *Value)
{
   DOUBLE val;
   if (Value->Type & FD_DOUBLE) val = Value->Double;
   else if (Value->Type & FD_LARGE) val = Value->Large;
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) {
      val = val * 0.01;
      Self->Dimensions = (Self->Dimensions | DMF_RELATIVE_RADIUS) & (~DMF_FIXED_RADIUS);
   }
   else Self->Dimensions = (Self->Dimensions | DMF_FIXED_RADIUS) & (~DMF_RELATIVE_RADIUS);

   Self->Radius = val;
   reset_path(Self);
   return ERR_Okay;
}

/*********************************************************************************************************************
-FIELD-
Repeat: Repeat the generated shape multiple times.

If set to a value greater than one, the Repeat field will cause the generated shape to be replicated multiple times
at consistent intervals leading to the center point.

The Repeat value cannot be set in conjunction with #Spiral.

*********************************************************************************************************************/

static ERROR SUPER_GET_Repeat(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Repeat;
   return ERR_Okay;
}

static ERROR SUPER_SET_Repeat(extVectorShape *Self, LONG Value)
{
   if ((Value >= 0) and (Value < 512)) {
      Self->Repeat = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Spiral: Alters the generated super shape so that it forms a spiral.

Setting the Spiral field to a value greater than one will cause the path generator to form spirals, up to the value
specified.  For instance, a value of 5 will generate five spirals.

*********************************************************************************************************************/

static ERROR SUPER_GET_Spiral(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Spiral;
   return ERR_Okay;
}

static ERROR SUPER_SET_Spiral(extVectorShape *Self, LONG Value)
{
   if (Value >= 0) {
      Self->Spiral = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Vertices: Limits the total number of vertices generated for the super shape.

Setting a value in Vertices will limit the total number of vertices that are generated for the super shape.  This feature
is useful for generating common convex geometrical shapes such as triangles, polygons, hexagons and so forth; because
their vertices will always touch the sides of an elliptical area.
-END-
*********************************************************************************************************************/

static ERROR SUPER_GET_Vertices(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Vertices;
   return ERR_Okay;
}

static ERROR SUPER_SET_Vertices(extVectorShape *Self, LONG Value)
{
   if ((Value >= 3) and (Value < 16384)) {
      Self->Vertices = Value;
      reset_path(Self);
      return ERR_Okay;
   }
   else return ERR_InvalidValue;
}

//********************************************************************************************************************

static const FieldDef clSuperDimensions[] = {
   { "FixedRadius",     DMF_FIXED_RADIUS },
   { "FixedCenterX",    DMF_FIXED_CENTER_X },
   { "FixedCenterY",    DMF_FIXED_CENTER_Y },
   { "RelativeRadius",  DMF_RELATIVE_RADIUS },
   { "RelativeCenterX", DMF_RELATIVE_CENTER_X },
   { "RelativeCenterY", DMF_RELATIVE_CENTER_Y },
   { NULL, 0 }
};

static const ActionArray clVectorShapeActions[] = {
   { AC_NewObject, SUPER_NewObject },
   { 0, NULL }
};

static const FieldArray clVectorShapeFields[] = {
   { "CenterX",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, SUPER_GET_CenterX, SUPER_SET_CenterX },
   { "CenterY",    FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, SUPER_GET_CenterY, SUPER_SET_CenterY },
   { "Radius",     FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, SUPER_GET_Radius,  SUPER_SET_Radius },
   { "Close",      FDF_VIRTUAL|FDF_LONG|FDF_RW, SUPER_GET_Close, SUPER_SET_Close },
   { "Dimensions", FDF_VIRTUAL|FDF_LONGFLAGS|FDF_RW, SUPER_GET_Dimensions, SUPER_SET_Dimensions, &clSuperDimensions },
   { "Phi",        FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_Phi,  SUPER_SET_Phi },
   { "A",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_A,  SUPER_SET_A },
   { "B",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_B,  SUPER_SET_B },
   { "M",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_M,  SUPER_SET_M },
   { "N1",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_N1, SUPER_SET_N1 },
   { "N2",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_N2, SUPER_SET_N2 },
   { "N3",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_N3, SUPER_SET_N3 },
   { "Vertices",   FDF_VIRTUAL|FDF_LONG|FDF_RW, SUPER_GET_Vertices, SUPER_SET_Vertices },
   { "Mod",        FDF_VIRTUAL|FDF_LONG|FDF_RW, SUPER_GET_Mod, SUPER_SET_Mod },
   { "Spiral",     FDF_VIRTUAL|FDF_LONG|FDF_RW, SUPER_GET_Spiral, SUPER_SET_Spiral },
   { "Repeat",     FDF_VIRTUAL|FDF_LONG|FDF_RW, SUPER_GET_Repeat, SUPER_SET_Repeat },
   // Synonyms
   { "CX", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, SUPER_GET_CenterX, SUPER_SET_CenterX },
   { "CY", FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, SUPER_GET_CenterY, SUPER_SET_CenterY },
   { "R",  FDF_SYNONYM|FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, SUPER_GET_Radius,  SUPER_SET_Radius },
   END_FIELD
};

//********************************************************************************************************************

static ERROR init_supershape(void)
{
   clVectorShape = objMetaClass::create::global(
      fl::BaseClassID(ID_VECTOR),
      fl::SubClassID(ID_VECTORSHAPE),
      fl::Name("VectorShape"),
      fl::Category(CCF_GRAPHICS),
      fl::Actions(clVectorShapeActions),
      fl::Fields(clVectorShapeFields),
      fl::Size(sizeof(extVectorShape)),
      fl::Path(MOD_PATH));

   return clVectorShape ? ERR_Okay : ERR_AddClass;
}

