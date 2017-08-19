# Compile everything in the 'modules' folder.

# Use wildcard to list all folders under 'modules'
ALL_MODULES = $(filter-out modules/makefile,$(sort $(wildcard modules/*/)))

# Use a recipe of 'clean compile' if recompiling from scratch
RECIPE ?= compile

compile: $(ALL_MODULES)

.PHONY: $(ALL_MODULES)

$(ALL_MODULES):
ifeq ($(RECIPE),doc)
	cd $@ && $(MAKE) -k -i -B ../../include/parasol/modules/$(subst /,,$(subst modules/,,$@)).h
else
	@$(MAKE) -C $@ $(RECIPE)
endif
