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
   static constexpr CLASSID CLASS_ID = CLASSID::VECTORSHAPE;
   static constexpr CSTRING CLASS_NAME = "VectorShape";
   using create = pf::Create<extVectorShape>;

   double Radius;
   double CX, CY;
   double M, N1, N2, N3, A, B, Phi;
   LONG Vertices;
   LONG Spiral;
   LONG Repeat;
   DMF Dimensions;
   bool Close;
   uint8_t Mod;
};

//********************************************************************************************************************

static void generate_supershape(extVectorShape *Vector, agg::path_storage &Path)
{
   double cx = Vector->CX, cy = Vector->CY;

   agg::path_storage path_buffer, *target;
   if (Path.empty()) target = &Path;
   else target = &path_buffer;

   if (dmf::hasScaledCenterX(Vector->Dimensions)) cx *= get_parent_width(Vector);
   if (dmf::hasScaledCenterY(Vector->Dimensions)) cy *= get_parent_height(Vector);

   const double scale = Vector->Radius;
   double rescale = 0;
   double tscale = Vector->Transform.scale();

   double vertices = Vector->Vertices;
   if (vertices IS DEFAULT_VERTICES) {
      if (Vector->Spiral > 1) vertices *= 2;
   }

   const double m  = Vector->M;
   const double n1 = Vector->N1;
   const double n2 = Vector->N2;
   const double n3 = Vector->N3;
   double phi_a;
   if (Vector->Spiral > 1) phi_a = (agg::pi * Vector->Phi * double(Vector->Spiral)) / vertices;
   else phi_a = (agg::pi * Vector->Phi) / vertices;
   const double a = 1.0 / Vector->A;
   const double b = 1.0 / Vector->B;
   double min_x = DBL_MAX, max_x = -DBL_MAX, min_y = DBL_MAX, max_y = -DBL_MAX;

   LONG lx = 0x7fffffff, ly = 0x7fffffff;
   for (double i=0; i < vertices; i++) {
      const double phi = phi_a * i;
      const double t1 = pow(std::abs(a * cos(m * phi * 0.25)), n2);
      const double t2 = pow(std::abs(b * sin(m * phi * 0.25)), n3);
      double r  = 1.0 / pow(t1 + t2, 1.0/n1);

      // These additional transforms can help in building a greater library of shapes.

      switch(Vector->Mod) {
         case 1: r = exp(r); break;
         case 2: r = log(r); break;
         case 3: r = atan(r); break;
         case 4: r = exp(1.0 / r); break;
         case 5: r = 1 + fastPow(cos(r), 2); break;
         case 6: r = fastPow(sin(r),2); break;
         case 7: r = 1 + fastPow(sin(r), 2); break;
         case 8: r = fastPow(cos(r),2); break;
      }

      double x = r * cos(phi);
      double y = r * sin(phi);

      x *= scale * tscale;
      y *= scale * tscale;

      // Prevent sub-pixel vertices from being generated.

      if ((F2I(x) IS lx) and (F2I(y) IS ly)) continue;
      lx = F2I(x);
      ly = F2I(y);

      // If x or y is greater than the radius, we'll have to rescale the final result after the shape has been generated.

      if (x > rescale) rescale = x;
      if (y > rescale) rescale = y;

      if (i == 0.0) target->move_to(x, y); // Plot the vertex
      else target->line_to(x, y);

      if (Vector->Spiral <= 1) { // Boundary management for non-spirals
         if (x < min_x) min_x = x;
         if (y < min_y) min_y = y;
         if (x > max_x) max_x = x;
         if (y > max_y) max_y = y;
      }
   }

   if (Vector->Spiral > 1) {
      double total = target->total_vertices();
      for (double i=0; i < total; i++) {
         double x, y;
         target->vertex(i, &x, &y);
         x = x * (i / total);
         y = y * (i / total);
         target->modify_vertex(i, x, y);

         // Boundary management

         if (x < min_x) min_x = x;
         if (y < min_y) min_y = y;
         if (x > max_x) max_x = x;
         if (y > max_y) max_y = y;
      }
   }
   else {
      if (Vector->Repeat > 1) {
         target->close_polygon(); // Repeated paths are always closed.

         agg::path_storage clone(*target);

         for (LONG i=0; i < Vector->Repeat-1; i++) {
            agg::trans_affine transform;
            transform.scale(double(i+1) / double(Vector->Repeat));
            agg::conv_transform<agg::path_storage, agg::trans_affine> scaled_path(clone, transform);
            target->concat_path(scaled_path);
         }
      }
      else if (Vector->Close) target->close_polygon();
   }

   agg::trans_affine transform;
   if (rescale != scale) transform.scale(scale / rescale);
   transform.translate(cx, cy);
   target->transform(transform);

   if (&Path != target) Path.concat_path(*target);

   Vector->Bounds = { min_x + cx, min_y + cy, max_x + cx, max_y + cy };
}

//********************************************************************************************************************

static ERR SUPER_NewObject(extVectorShape *Self)
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
   Self->Close = true;
   Self->GeneratePath = (void (*)(extVector *, agg::path_storage &))&generate_supershape;
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
A: A parameter for the Superformula.

This field sets the Superformula's 'A' parameter value.

*********************************************************************************************************************/

static ERR SUPER_GET_A(extVectorShape *Self, double *Value)
{
   *Value = Self->A;
   return ERR::Okay;
}

static ERR SUPER_SET_A(extVectorShape *Self, double Value)
{
   Self->A = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
B: A parameter for the Superformula.

This field sets the Superformula's 'B' parameter value.

*********************************************************************************************************************/

static ERR SUPER_GET_B(extVectorShape *Self, double *Value)
{
   *Value = Self->B;
   return ERR::Okay;
}

static ERR SUPER_SET_B(extVectorShape *Self, double Value)
{
   Self->B = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterX: The center of the shape on the x-axis.  Expressed as a fixed or scaled coordinate.

The horizontal center of the shape is defined here as either a fixed or scaled value.

*********************************************************************************************************************/

static ERR SUPER_GET_CenterX(extVectorShape *Self, Unit *Value)
{
   Value->set(Self->CX);
   return ERR::Okay;
}

static ERR SUPER_SET_CenterX(extVectorShape *Self, Unit &Value)
{
   if (Value.scaled()) Self->Dimensions = (Self->Dimensions | DMF::SCALED_CENTER_X) & (~DMF::FIXED_CENTER_X);
   else Self->Dimensions = (Self->Dimensions | DMF::FIXED_CENTER_X) & (~DMF::SCALED_CENTER_X);
   Self->CX = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
CenterY: The center of the shape on the y-axis.  Expressed as a fixed or scaled coordinate.

The vertical center of the shape is defined here as either a fixed or scaled value.

*********************************************************************************************************************/

static ERR SUPER_GET_CenterY(extVectorShape *Self, Unit *Value)
{
   Value->set(Self->CY);
   return ERR::Okay;
}

static ERR SUPER_SET_CenterY(extVectorShape *Self, Unit &Value)
{
   if (Value.scaled()) Self->Dimensions = (Self->Dimensions | DMF::SCALED_CENTER_Y) & (~DMF::FIXED_CENTER_Y);
   else Self->Dimensions = (Self->Dimensions | DMF::FIXED_CENTER_Y) & (~DMF::SCALED_CENTER_Y);
   Self->CY = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Close: A parameter for the super shape algorithm.

If TRUE, the shape path will be closed between the beginning and end points.

*********************************************************************************************************************/

static ERR SUPER_GET_Close(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Close;
   return ERR::Okay;
}

static ERR SUPER_SET_Close(extVectorShape *Self, LONG Value)
{
   Self->Close = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Dimensions: Dimension flags define whether individual dimension fields contain fixed or scaled values.

The following dimension flags are supported:

<types lookup="DMF">
<type name="FIXED_CENTER_X">The #CenterX value is a fixed coordinate.</>
<type name="FIXED_CENTER_Y">The #CenterY value is a fixed coordinate.</>
<type name="FIXED_RADIUS">The #Radius value is a fixed coordinate.</>
<type name="SCALED_CENTER_X">The #CenterX value is a scaled coordinate.</>
<type name="SCALED_CENTER_Y">The #CenterY value is a scaled coordinate.</>
<type name="SCALED_RADIUS">The #Radius value is a scaled coordinate.</>
</types>

*********************************************************************************************************************/

static ERR SUPER_GET_Dimensions(extVectorShape *Self, DMF *Value)
{
   *Value = Self->Dimensions;
   return ERR::Okay;
}

static ERR SUPER_SET_Dimensions(extVectorShape *Self, DMF Value)
{
   Self->Dimensions = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
M: A parameter for the Superformula.

This field sets the Superformula's 'M' parameter value.

*********************************************************************************************************************/

static ERR SUPER_GET_M(extVectorShape *Self, double *Value)
{
   *Value = Self->M;
   return ERR::Okay;
}

static ERR SUPER_SET_M(extVectorShape *Self, double Value)
{
   Self->M = Value;
   reset_path(Self);
   return ERR::Okay;
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

static ERR SUPER_GET_Mod(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Mod;
   return ERR::Okay;
}

static ERR SUPER_SET_Mod(extVectorShape *Self, LONG Value)
{
   Self->Mod = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
N1: A parameter for the super shape algorithm.

This field sets the Superformula's 'N1' parameter value.

*********************************************************************************************************************/

static ERR SUPER_GET_N1(extVectorShape *Self, double *Value)
{
   *Value = Self->N1;
   return ERR::Okay;
}

static ERR SUPER_SET_N1(extVectorShape *Self, double Value)
{
   Self->N1 = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
N2: A parameter for the super shape algorithm.

This field sets the Superformula's 'N2' parameter value.

*********************************************************************************************************************/

static ERR SUPER_GET_N2(extVectorShape *Self, double *Value)
{
   *Value = Self->N2;
   return ERR::Okay;
}

static ERR SUPER_SET_N2(extVectorShape *Self, double Value)
{
   Self->N2 = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
N3: A parameter for the super shape algorithm.

This field sets the Superformula's 'N3' parameter value.

*********************************************************************************************************************/

static ERR SUPER_GET_N3(extVectorShape *Self, double *Value)
{
   *Value = Self->N3;
   return ERR::Okay;
}

static ERR SUPER_SET_N3(extVectorShape *Self, double Value)
{
   Self->N3 = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Phi: A parameter for the super shape algorithm.

The Phi value has an impact on the length of the generated path.  If the super shape parameters form a circular path
(whereby the last vertex meets the first) then the Phi value should not be modified.  If the path does not meet
itself then the Phi value should be increased until it does.  The minimum (and default) value is 2.  It is recommended
that the Phi value is increased in increments of 2 until the desired effect is achieved.

*********************************************************************************************************************/

static ERR SUPER_GET_Phi(extVectorShape *Self, double *Value)
{
   *Value = Self->Phi;
   return ERR::Okay;
}

static ERR SUPER_SET_Phi(extVectorShape *Self, double Value)
{
   if (Value >= 2.0) {
      Self->Phi = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Radius: The radius of the generated shape.  Expressed as a fixed or scaled coordinate.

The Radius defines the final size of the generated shape.  It can be expressed in fixed or scaled terms.

*********************************************************************************************************************/

static ERR SUPER_GET_Radius(extVectorShape *Self, Unit *Value)
{
   Value->set(Self->Radius);
   return ERR::Okay;
}

static ERR SUPER_SET_Radius(extVectorShape *Self, Unit &Value)
{
   if (Value.scaled()) Self->Dimensions = (Self->Dimensions|DMF::SCALED_RADIUS_X|DMF::SCALED_RADIUS_Y) & (~(DMF::FIXED_RADIUS_X|DMF::FIXED_RADIUS_Y));
   else Self->Dimensions = (Self->Dimensions|DMF::FIXED_RADIUS_X|DMF::FIXED_RADIUS_Y) & (~(DMF::SCALED_RADIUS_X|DMF::SCALED_RADIUS_Y));

   Self->Radius = Value;
   reset_path(Self);
   return ERR::Okay;
}

/*********************************************************************************************************************
-FIELD-
Repeat: Repeat the generated shape multiple times.

If set to a value greater than one, the Repeat field will cause the generated shape to be replicated multiple times
at consistent intervals leading to the center point.

The Repeat value cannot be set in conjunction with #Spiral.

*********************************************************************************************************************/

static ERR SUPER_GET_Repeat(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Repeat;
   return ERR::Okay;
}

static ERR SUPER_SET_Repeat(extVectorShape *Self, LONG Value)
{
   if ((Value >= 0) and (Value < 512)) {
      Self->Repeat = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Spiral: Alters the generated super shape so that it forms a spiral.

Setting the Spiral field to a value greater than one will cause the path generator to form spirals, up to the value
specified.  For instance, a value of 5 will generate five spirals.

*********************************************************************************************************************/

static ERR SUPER_GET_Spiral(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Spiral;
   return ERR::Okay;
}

static ERR SUPER_SET_Spiral(extVectorShape *Self, LONG Value)
{
   if (Value >= 0) {
      Self->Spiral = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

/*********************************************************************************************************************
-FIELD-
Vertices: Limits the total number of vertices generated for the super shape.

Setting a value in Vertices will limit the total number of vertices that are generated for the super shape.  This feature
is useful for generating common convex geometrical shapes such as triangles, polygons, hexagons and so forth; because
their vertices will always touch the sides of an elliptical area.
-END-
*********************************************************************************************************************/

static ERR SUPER_GET_Vertices(extVectorShape *Self, LONG *Value)
{
   *Value = Self->Vertices;
   return ERR::Okay;
}

static ERR SUPER_SET_Vertices(extVectorShape *Self, LONG Value)
{
   if ((Value >= 3) and (Value < 16384)) {
      Self->Vertices = Value;
      reset_path(Self);
      return ERR::Okay;
   }
   else return ERR::InvalidValue;
}

//********************************************************************************************************************

static const FieldDef clSuperDimensions[] = {
   { "FixedCenterX",  DMF::FIXED_CENTER_X },
   { "FixedCenterY",  DMF::FIXED_CENTER_Y },
   { "ScaledCenterX", DMF::SCALED_CENTER_X },
   { "ScaledCenterY", DMF::SCALED_CENTER_Y },
   { nullptr, 0 }
};

static const ActionArray clVectorShapeActions[] = {
   { AC::NewObject, SUPER_NewObject },
   { AC::NIL, nullptr }
};

static const FieldArray clVectorShapeFields[] = {
   { "CenterX",    FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, SUPER_GET_CenterX, SUPER_SET_CenterX },
   { "CenterY",    FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, SUPER_GET_CenterY, SUPER_SET_CenterY },
   { "Radius",     FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, SUPER_GET_Radius,  SUPER_SET_Radius },
   { "Close",      FDF_VIRTUAL|FDF_INT|FDF_RW, SUPER_GET_Close, SUPER_SET_Close },
   { "Dimensions", FDF_VIRTUAL|FDF_INTFLAGS|FDF_RW, SUPER_GET_Dimensions, SUPER_SET_Dimensions, &clSuperDimensions },
   { "Phi",        FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_Phi,  SUPER_SET_Phi },
   { "A",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_A,  SUPER_SET_A },
   { "B",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_B,  SUPER_SET_B },
   { "M",          FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_M,  SUPER_SET_M },
   { "N1",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_N1, SUPER_SET_N1 },
   { "N2",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_N2, SUPER_SET_N2 },
   { "N3",         FDF_VIRTUAL|FDF_DOUBLE|FDF_RW, SUPER_GET_N3, SUPER_SET_N3 },
   { "Vertices",   FDF_VIRTUAL|FDF_INT|FDF_RW, SUPER_GET_Vertices, SUPER_SET_Vertices },
   { "Mod",        FDF_VIRTUAL|FDF_INT|FDF_RW, SUPER_GET_Mod, SUPER_SET_Mod },
   { "Spiral",     FDF_VIRTUAL|FDF_INT|FDF_RW, SUPER_GET_Spiral, SUPER_SET_Spiral },
   { "Repeat",     FDF_VIRTUAL|FDF_INT|FDF_RW, SUPER_GET_Repeat, SUPER_SET_Repeat },
   // Synonyms
   { "CX", FDF_SYNONYM|FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, SUPER_GET_CenterX, SUPER_SET_CenterX },
   { "CY", FDF_SYNONYM|FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, SUPER_GET_CenterY, SUPER_SET_CenterY },
   { "R",  FDF_SYNONYM|FDF_VIRTUAL|FDF_UNIT|FDF_DOUBLE|FDF_SCALED|FDF_RW, SUPER_GET_Radius,  SUPER_SET_Radius },
   END_FIELD
};

//********************************************************************************************************************

static ERR init_supershape(void)
{
   clVectorShape = objMetaClass::create::global(
      fl::BaseClassID(CLASSID::VECTOR),
      fl::ClassID(CLASSID::VECTORSHAPE),
      fl::Name("VectorShape"),
      fl::Category(CCF::GRAPHICS),
      fl::Actions(clVectorShapeActions),
      fl::Fields(clVectorShapeFields),
      fl::Size(sizeof(extVectorShape)),
      fl::Path(MOD_PATH));

   return clVectorShape ? ERR::Okay : ERR::AddClass;
}

