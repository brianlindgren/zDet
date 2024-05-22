
# Your external's name
lib.name = zDect~

# Source files
class.sources = src/zDect~.cpp

PDLIBBUILDER_DIR=pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

buildcheck: all
	test -e _template_.$(extension)
installcheck: install
	test -e $(installpath)/_template_.$(extension)
