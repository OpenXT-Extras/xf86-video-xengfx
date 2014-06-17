/**************************************************************************
 *
 * Copyright (c) 2012 Citrix Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Aurelien Chartier <chartier.aurelien@gmail.com>
 *
 **************************************************************************/

#ifndef XENGFX_DRIVER_H_
#define XENGFX_DRIVER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xorg-server.h>

#include <errno.h>
#include <xf86.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <xf86Crtc.h>

#define XENGFX_VERSION_MAJOR PACKAGE_VERSION_MAJOR
#define XENGFX_VERSION_MINOR PACKAGE_VERSION_MINOR
#define XENGFX_VERSION_PATCH PACKAGE_VERSION_PATCHLEVEL

#define XENGFX_VERSION 1
#define XENGFX_NAME "xengfx"
#define XENGFX_DRIVER_NAME "xengfx"

#define XENGFX_VENDOR_ID 0x5853
#define XENGFX_DEVICE_ID 0xc147

struct xengfx_bo
{
    uint32_t handle;
    uint32_t size;
    void *ptr;
    int map_count;
    uint32_t pitch;
};

struct xengfx_drm_mode
{
    int fd;
    unsigned fb_id;
    drmModeResPtr mode_res;
    ScrnInfoPtr scrn;
    int cpp;

    struct xengfx_bo *front_bo;
};


struct xengfx_crtc
{
    drmModeCrtcPtr mode_crtc;
    drmModeModeInfo kmode;
    struct xengfx_drm_mode *drm_mode;

    struct xengfx_bo *cursor_bo;

    struct xengfx_bo *rotate_bo;
    uint32_t rotate_fb_id;
    uint32_t rotate_pitch;
};


struct xengfx_property
{
    drmModePropertyPtr mode_prop;
    uint64_t value;
    int num_atoms;
    Atom *atoms;
};


struct xengfx_output
{
    struct xengfx_drm_mode *mode;
    int output_id;
    drmModeConnectorPtr mode_output;
    drmModeEncoderPtr mode_encoder;

    int num_props;
    struct xengfx_property *props;
};


struct xengfx_private
{
    int fd;
    EntityInfoPtr pEnt;
    struct pci_device *PciInfo;
    OptionInfoPtr Options;
    struct xengfx_drm_mode mode;

    // Backup of original functions to restore
    CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr CloseScreen;
    ScreenBlockHandlerProcPtr BlockHandler;

    DamagePtr damage;
};

#define to_xengfx_private(p) ((struct xengfx_private*)(p->driverPrivate))

// xengfx_crtc
void xengfx_crtc_init(ScrnInfoPtr scrn, struct xengfx_drm_mode *drm_mode, int num);
Bool xengfx_crtc_resize(ScrnInfoPtr scrn, int width, int height);

//xengfx_drm
Bool xengfx_drm_pre_init(ScrnInfoPtr scrn, struct xengfx_drm_mode *mode, int cpp);
Bool xengfx_drm_set_desired_modes(ScrnInfoPtr scrn, struct xengfx_drm_mode *mode);
void xengfx_mode_to_kmode(drmModeModeInfoPtr kmode, DisplayModePtr mode);
void xengfx_mode_from_kmode(ScrnInfoPtr scrn, drmModeModeInfoPtr kmode, DisplayModePtr mode);
struct xengfx_bo* xengfx_drm_create_bo(int fd, const unsigned width, const unsigned height, const unsigned bpp);
int xengfx_drm_map_bo(int fd, struct xengfx_bo *bo);
int xengfx_drm_destroy_bo(int fd, struct xengfx_bo *bo);
void* xengfx_drm_map_front_bo(struct xengfx_drm_mode *drm_mode);
Bool xengfx_drm_create_initial_bos(ScrnInfoPtr scrn, struct xengfx_drm_mode *drm_mode);

//xengfx_output
void xengfx_output_init(ScrnInfoPtr scrn, struct xengfx_drm_mode *mode, int num);

#endif /* XENGFX_DRIVER_H */
