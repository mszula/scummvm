MODULE := engines/gearhead

MODULE_OBJS = \
	airratchet.o \
	archive.o \
	autoscript.o \
	car.o \
	cardata.o \
	carview.o \
	font.o \
	gearhead.o \
	gfx.o \
	input.o \
	jobengine.o \
	jobs.o \
	mechanic.o \
	metaengine.o \
	partsbin.o \
	renderer3d.o \
	resources.o \
	screens.o \
	signin.o \
	sound.o \
	tlv.o \
	ui.o \
	workshop.o

# This module can be built as a plugin
ifeq ($(ENABLE_GEARHEAD), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
