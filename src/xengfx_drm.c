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

#include <sys/mman.h>

#include "xengfx_driver.h"
#include "xengfx_drm.h"


void
xengfx_mode_to_kmode(drmModeModeInfoPtr kmode, DisplayModePtr mode)
{
    memset(kmode, 0, sizeof (*kmode));

    kmode->clock = mode->Clock;

    kmode->hdisplay = mode->HDisplay;
    kmode->hsync_start = mode->HSyncStart;
    kmode->hsync_end = mode->HSyncEnd;
    kmode->htotal = mode->HTotal;
    kmode->hskew = mode->HSkew;

    kmode->vdisplay = mode->VDisplay;
    kmode->vsync_start = mode->VSyncStart;
    kmode->vsync_end = mode->VSyncEnd;
    kmode->vtotal = mode->VTotal;
    kmode->vscan = mode->VScan;

    kmode->flags = mode->Flags;
    if (mode->name)
        strncpy(kmode->name, mode->name, DRM_DISPLAY_MODE_LEN);
    kmode->name[DRM_DISPLAY_MODE_LEN - 1] = 0;
}


void
xengfx_mode_from_kmode(ScrnInfoPtr scrn, drmModeModeInfoPtr kmode, DisplayModePtr mode)
{
    memset(mode, 0, sizeof (*mode));
    mode->status = MODE_OK;

    mode->Clock = kmode->clock;

    mode->HDisplay = kmode->hdisplay;
    mode->HSyncStart = kmode->hsync_start;
    mode->HSyncEnd = kmode->hsync_end;
    mode->HTotal = kmode->htotal;
    mode->HSkew = kmode->hskew;

    mode->VDisplay = kmode->vdisplay;
    mode->VSyncStart = kmode->vsync_start;
    mode->VSyncEnd = kmode->vsync_end;
    mode->VTotal = kmode->vtotal;
    mode->VScan = kmode->vscan;

    mode->Flags = kmode->flags;
    mode->name = strdup(kmode->name);

    if (kmode->type & DRM_MODE_TYPE_DRIVER)
        mode->type = M_T_DRIVER;
    if (kmode->type & DRM_MODE_TYPE_PREFERRED)
        mode->type |= M_T_PREFERRED;
    xf86SetModeCrtc(mode, scrn->adjustFlags);
}


struct xengfx_bo*
xengfx_drm_create_bo(int fd, const unsigned width, const unsigned height, const unsigned bpp)
{
    struct drm_xengfx_gem_create arg;
    struct xengfx_bo *bo;
    int ret;

    bo = calloc(1, sizeof (*bo));
    if (!bo)
        return NULL;

    memset(&arg, 0, sizeof (arg));
    arg.width = width;
    arg.height = height;
    arg.bpp = bpp;

    ret = drmIoctl(fd, DRM_IOCTL_XENGFX_GEM_CREATE, &arg);
    if (ret)
        goto err;

    bo->handle = arg.handle;
    bo->pitch = arg.pitch;
    bo->size = arg.size;

    return bo;
err:
    free(bo);
    return NULL;
}


Bool
xengfx_drm_create_initial_bos(ScrnInfoPtr scrn, struct xengfx_drm_mode *drm_mode)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    int width = scrn->virtualX;
    int height = scrn->virtualY;
    int bpp = scrn->bitsPerPixel;
    int cpp = (bpp + 7) / 8;
    int i;

    drm_mode->front_bo = xengfx_drm_create_bo(drm_mode->fd, width, height, bpp);
    if (!drm_mode->front_bo)
        return FALSE;
    scrn->displayWidth = drm_mode->front_bo->pitch / cpp;

    width = height = 64;
    bpp = 32;
    for (i = 0; i < xf86_config->num_crtc; ++i)
    {
        xf86CrtcPtr crtc = xf86_config->crtc[i];
        struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
        xengfx_crtc->cursor_bo = xengfx_drm_create_bo(drm_mode->fd, width, height, bpp);
    }

    return TRUE;
}


int
xengfx_drm_map_bo(int fd, struct xengfx_bo *bo)
{
    struct drm_xengfx_gem_map arg;
    int ret;
    void *map;

    if (bo->ptr)
    {
        // already mapped
        bo->map_count++;
        return 0;
    }

    memset(&arg, 0, sizeof (arg));
    arg.handle = bo->handle;

    ret = drmIoctl(fd, DRM_IOCTL_XENGFX_GEM_MAP, &arg);
    if (ret)
        return ret;

    map = mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, arg.offset);
    if (map == MAP_FAILED)
        return -errno;

    bo->ptr = map;
    return 0;
}


int
xengfx_drm_destroy_bo(int fd, struct xengfx_bo *bo)
{
    struct drm_gem_close arg;
    int ret;

    if (bo->ptr)
    {
        munmap(bo->ptr, bo->size);
        bo->ptr = NULL;
    }

    memset(&arg, 0, sizeof (arg));
    arg.handle = bo->handle;

    ret = drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &arg);
    if (ret)
        return -errno;

    free(bo);
    return 0;
}


void*
xengfx_drm_map_front_bo(struct xengfx_drm_mode *drm_mode)
{
    int ret;

    if (drm_mode->front_bo->ptr)
        return drm_mode->front_bo->ptr;

    ret = xengfx_drm_map_bo(drm_mode->fd, drm_mode->front_bo);
    if (ret)
        return NULL;

    return drm_mode->front_bo->ptr;
}


static const xf86CrtcConfigFuncsRec xengfx_crtc_config_funcs = {
    xengfx_crtc_resize
};


Bool
xengfx_drm_pre_init(ScrnInfoPtr scrn, struct xengfx_drm_mode *mode, int cpp)
{
    struct xengfx_private *xengfx = to_xengfx_private(scrn);
    int i;

    xf86CrtcConfigInit(scrn, &xengfx_crtc_config_funcs);

    mode->fd = xengfx->fd;
    mode->scrn = scrn;
    mode->cpp = cpp;
    mode->mode_res = drmModeGetResources(mode->fd);
    if (!mode->mode_res)
        return FALSE;

    xf86CrtcSetSizeRange(scrn, 320, 200, mode->mode_res->max_width, mode->mode_res->max_height);

    for (i = 0; i < mode->mode_res->count_crtcs; ++i)
        xengfx_crtc_init(scrn, mode, i);
    for (i = 0; i < mode->mode_res->count_connectors; ++i)
        xengfx_output_init(scrn, mode, i);

    xf86InitialConfiguration(scrn, TRUE);

    return TRUE;
}


Bool
xengfx_drm_set_desired_modes(ScrnInfoPtr scrn, struct xengfx_drm_mode *drm_mode)
{
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
    int i;

    for (i = 0; i < config->num_crtc; ++i)
    {
        xf86CrtcPtr crtc = config->crtc[i];
        struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
        xf86OutputPtr output = NULL;
        int j;

        // Skip disabled CRTCs
        if (!crtc->enabled)
        {
            drmModeSetCrtc(drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id,
                           0, 0, 0, NULL, 0, NULL);
            continue;
        }

        if (config->output[config->compat_output]->crtc == crtc)
            output = config->output[config->compat_output];
        else
        {
            for (j = 0; j < config->num_output; ++j)
                if (config->output[j]->crtc == crtc)
                {
                    output = config->output[j];
                    break;
                }
        }

        // paranoia
        if (!output)
            continue;

        // Mark that we'll need to re-set the mode for sure
        memset(&crtc->mode, 0, sizeof (crtc->mode));
        if (!crtc->desiredMode.CrtcHDisplay)
        {
            DisplayModePtr mode = xf86OutputFindClosestMode(output, scrn->currentMode);

            if (!mode)
                return FALSE;
            crtc->desiredMode = *mode;
            crtc->desiredRotation = RR_Rotate_0;
            crtc->desiredX = 0;
            crtc->desiredY = 0;
        }

        if (!crtc->funcs->set_mode_major(crtc, &crtc->desiredMode, crtc->desiredRotation,
                                         crtc->desiredX, crtc->desiredY))
            return FALSE;
    }

    return TRUE;
}
