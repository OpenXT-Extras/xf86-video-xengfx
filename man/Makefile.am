drivermandir = $(DRIVER_MAN_DIR)
driverman_DATA = $(DRIVER_NAME).$(DRIVER_MAN_SUFFIX)

EXTRA_DIST = $(DRIVER_NAME).man

CLEANFILES = $(driverman_DATA)

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure


SUFFIXES = .$(DRIVER_MAN_SUFFIX) .man

.man.$(DRIVER_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

