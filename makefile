# Please refer to the readme.md file for instructions prior to attempting your first build.

include makefile.inc

VERIFY_SCRIPT := scripts/verify-modules.fluid

# Comment out PARALLEL_BUILD if you would prefer a log that is easier to read or debug, at a cost of speed.

ifeq (4.1,$(firstword $(sort $(MAKE_VERSION) 4.1)))
PARALLEL_BUILD := -j8 --output-sync=recurse
else
PARALLEL_BUILD := -j8
endif

# Simple build: Compiles source code and resets configuration files.  No pre-clean is performed.

compile:
	$(MAKE) $(PARALLEL_BUILD) -C core -f makefile
	$(MAKE) config

3rdparty:
	RECIPE="clean compile" $(MAKE) -C core/3rdparty -f makefile

# Full build: Clean everything and prepare a build suitable for binary release.

full-compile:
	mkdir -p "$(PARASOL_MODULES)"
	$(MAKE) -C core/link -f makefile clean
	$(MAKE) -C core/link -f makefile
	$(MAKE) -C core/src -f makefile clean
	$(MAKE) -C core/src -f makefile
	RECIPE="clean" $(MAKE) -C core -f makefile
	$(MAKE) $(PARALLEL_BUILD) -C core -f makefile
	$(MAKE) -C core/launcher -f makefile clean
	$(MAKE) -C core/launcher -f makefile
	$(MAKE) config
	"$(PARASOL_RELEASE)/parasol-cmd" --log-error $(VERIFY_SCRIPT)

# Generate documentation for modules and classes.

documents:
	$(MAKE) -B -C core/src -f makefile  ../include/parasol/modules/core.h
	RECIPE="doc" $(MAKE) -i -k -C core

verify:
	"$(PARASOL_RELEASE)/parasol-cmd" --log-error $(VERIFY_SCRIPT)

# Recompile everything from scratch.  The 'RECIPE' is executed by the makefile in core

# Copies the configuration files to release/

.FORCE:
config: .FORCE
	rsync -avh --delete data/config "$(PARASOL_RELEASE)/system/"
	rsync -avh --delete data/fonts "$(PARASOL_RELEASE)/system/"
	rsync -avh --delete data/icons "$(PARASOL_RELEASE)/system/"
	rsync -avh --delete data/scripts "$(PARASOL_RELEASE)/system/"

# Create an installation folder that mirrors the release build.  Use 'clean-install' if the existing installation
# folder should be cleared of any existing non-release files.

install: .FORCE
	rsync -avh "$(PARASOL_RELEASE)/" "$(PARASOL_INSTALL)"

clean-install:
	rsync -avh --delete "$(PARASOL_RELEASE)/" "$(PARASOL_INSTALL)"

all: full-compile install
