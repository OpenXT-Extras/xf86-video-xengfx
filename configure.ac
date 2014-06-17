# Initialize Autoconf
AC_INIT([xf86-video-xengfx], [0.1.0])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Initialize X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

AC_ARG_WITH(xorg-module-dir,
            AS_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg/modules]]]),
                           [moduledir="$withval"],
                           [moduledir="$libdir/xorg/modules"])

# debug flags : always nice for GDB
AC_ARG_WITH([debug],
            [AS_HELP_STRING([--with-debug],
                            [Adding -g and -ggdb flags tio the compilation])],
            [CFLAGS="$CFLAGS -O0 -g -ggdb"],
            [CFLAGS="$CFLAGS -O2 -DNDEBUG"])

# Check for extensions
XORG_DRIVER_CHECK_EXT(RANDR, randrproto)

# Checks for pkg-config packages
PKG_CHECK_MODULES(XORG, [xorg-server xproto fontsproto $REQUIRED_MODULES])
sdkdir=$(pkg-config --variable=sdkdir xorg-server)

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

PKG_CHECK_MODULES(DRM, [libdrm >= 2.2])
PKG_CHECK_MODULES([PCIACCESS], [pciaccess >= 0.10])
AM_CONDITIONAL(DRM, test "x$DRM" = xyes)

PKG_CHECK_MODULES(UDEV, [libudev], [udev=yes], [udev=no])
if test x"$udev" = xyes; then
    AC_DEFINE(HAVE_UDEV,1,[Enable udev-based monitor hotplug detection])
fi

DRIVER_NAME=xengfx
AC_SUBST([DRIVER_NAME])
AC_SUBST([moduledir])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	man/Makefile
])

AC_OUTPUT
