
include ../../core.mk
# The make process is split into two files so as to allow a parallel build.  This file performs the final link stage only.

NAME=     vector
CATEGORY= graphics
DEST=     $(PARASOL_MODULE)
AGG=      agg/src/
DOCUMENTS= ./ defs/colour.cpp defs/gradient.cpp defs/image.cpp defs/pattern.cpp defs/transition.cpp filters/filter.cpp scene/scene.cpp vectors/clip.c vectors/ellipse.c vectors/group.c vectors/path.c vectors/polygon.c vectors/rectangle.c vectors/spiral.cpp vectors/supershape.cpp vectors/text.c vectors/vector.cpp vectors/viewport.c vectors/wave.cpp
HEADER= ../../include/parasol/modules/$(NAME).h
CFLAGS += -DMOD_NAME=$(NAME)
CXXFLAGS += -DMOD_NAME=$(NAME)

SRC_CXX=vector.cpp \
$(AGG)agg_arc.cpp \
$(AGG)agg_bezier_arc.cpp \
$(AGG)agg_curves.cpp \
$(AGG)agg_vcgen_dash.cpp \
$(AGG)agg_vcgen_stroke.cpp \
$(AGG)agg_image_filters.cpp \
$(AGG)agg_line_aa_basics.cpp \
$(AGG)agg_rounded_rect.cpp \
$(AGG)agg_sqrt_tables.cpp \
$(AGG)agg_trans_affine.cpp \
$(AGG)agg_trans_single_path.cpp
#$(AGG)agg_bspline.cpp \
#$(AGG)agg_vcgen_contour.cpp \
#$(AGG)agg_vcgen_bspline.cpp \
#$(AGG)agg_vcgen_smooth_poly1.cpp \
#$(AGG)agg_line_profile_aa.cpp \
#$(AGG)agg_vpgen_clip_polygon.cpp
#$(AGG)agg_vpgen_clip_polyline.cpp
#$(AGG)agg_vpgen_segmentator.cpp
#$(AGG)agg_trans_double_path.cpp \
#$(AGG)agg_arrowhead.cpp \
#$(AGG)agg_vcgen_markers_term.cpp \

OBJ=$(SRC_CXX:.cpp=.o) $(SRC_C:.c=.o)

-include deps-$(OSTYPE).mak

# For MinGW the -static-libstdc++ option can be used to produce a DLL independent of the std++ DLL, but this blows out
# the final binary size by an extra 1.47MB.

compile: $(HEADER) idl.h module_def.c
	make -j -f makefile_agg
	$(CXX) $(LIBLINK_STDCPP) $(OBJ) -lm

$(HEADER) module_def.c idl.h: $(NAME).fdl vector_functions.cpp defs/colour.cpp defs/gradient.cpp defs/image.cpp defs/pattern.cpp defs/transition.cpp filters/filter.cpp scene/scene.cpp vectors/clip.c vectors/ellipse.c vectors/group.c vectors/path.c vectors/polygon.c vectors/rectangle.c vectors/spiral.cpp vectors/supershape.cpp vectors/text.c vectors/vector.cpp vectors/viewport.c vectors/wave.cpp
	$(IDL_C) --src=$< --files={ vector_functions.cpp } --output-defs=module_def.c
	$(IDL_DEF) --src=$< --output=idl.h --format=c

clean:
	@rm -f $(OBJ) *.mak
	@touch $(NAME).fdl
