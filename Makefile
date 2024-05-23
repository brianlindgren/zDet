# Name of your external library
lib.name = zDet~

# Source files
class.sources = src/zDet~.cpp

# Help file (and any other data files)
datafiles = zDet~-help.pd

# Path to pd-lib-builder
PDLIBBUILDER_DIR = pd-lib-builder

# Include pd-lib-builder makefile
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

# Custom targets for build and install checks
buildcheck: all
	test -e zDet~.$(extension)

installcheck: install
	test -e $(installpath)/zDet~.$(extension)
