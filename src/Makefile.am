#  Copyright (c) 2011 Citrix Systems, Inc.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

XENGFX_WARN_FLAGS = -Wall -Wpointer-arith -Wmissing-declarations -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -Wbad-function-cast -Wold-style-definition -Wdeclaration-after-statement -Wunused -Wuninitialized -Wshadow -Wcast-qual -Wmissing-noreturn -Wmissing-format-attribute -Werror=implicit -Werror=nonnull -Werror=init-self -Werror=main -Werror=missing-braces -Werror=sequence-point -Werror=return-type -Werror=trigraphs -Werror=array-bounds -Werror=address -Werror=int-to-pointer-cast -Werror=pointer-to-int-cast -fno-strict-aliasing

AM_CFLAGS = $(XORG_CFLAGS) $(DRM_CFLAGS) $(XENGFX_WARN_FLAGS)

xengfx_drv_la_LTLIBRARIES = xengfx_drv.la
xengfx_drv_la_LDFLAGS = -module -avoid-version
xengfx_drv_la_LIBADD = @UDEV_LIBS@ @DRM_LIBS@
xengfx_drv_ladir = @moduledir@/drivers

xengfx_drv_la_SOURCES = \
	 xengfx_driver.c \
	 xengfx_drm.c \
	 xengfx_crtc.c \
	 xengfx_output.c

