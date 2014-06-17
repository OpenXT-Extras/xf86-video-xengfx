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

#include "xengfx_driver.h"

static void
xengfx_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    // XXX: this is not really critical right now
}


static Bool
xengfx_crtc_apply(xf86CrtcPtr crtc)
{
    ScrnInfoPtr scrn = crtc->scrn;
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;
    int i, fb_id, ret = FALSE;

    uint32_t *output_ids;
    int output_count = 0;

    output_ids = calloc(sizeof (uint32_t), xf86_config->num_output);
    if (!output_ids)
        return FALSE;

    for (i = 0; i < xf86_config->num_output; ++i)
    {
        xf86OutputPtr output = xf86_config->output[i];
        struct xengfx_output *xengfx_output;
        if (output->crtc != crtc)
            continue;

        xengfx_output = output->driver_private;
        output_ids[output_count++] = xengfx_output->mode_output->connector_id;
    }

    if (!xf86CrtcRotate(crtc))
        return FALSE;

    crtc->funcs->gamma_set(crtc, crtc->gamma_red, crtc->gamma_green,
                           crtc->gamma_blue, crtc->gamma_size);

    fb_id = drm_mode->fb_id;
    ret = drmModeSetCrtc(drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id,
                         fb_id, crtc->x, crtc->y, output_ids, output_count,
                         &xengfx_crtc->kmode);

    if (ret)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "failed to set mode : %s\n",
                   strerror(-ret));
        ret = FALSE;
    }
    else
        ret = TRUE;

    return ret;
}


static Bool
xengfx_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
                           Rotation rotation, int x, int y)
{
    ScrnInfoPtr scrn = crtc->scrn;
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;
    Bool ret = TRUE;

    DisplayModeRec saved_mode;
    Rotation saved_rotation;
    int saved_x, saved_y;

    if (drm_mode->fb_id == 0)
    {
        ret = drmModeAddFB(drm_mode->fd,
                           scrn->virtualX, scrn->virtualY,
                           scrn->depth, scrn->bitsPerPixel,
                           drm_mode->front_bo->pitch,
                           drm_mode->front_bo->handle,
                           &drm_mode->fb_id);
        if (ret < 0) {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                       "failed to add fb %d\n", ret);
            return FALSE;
        }
    }

    saved_mode = crtc->mode;
    saved_rotation = crtc->rotation;
    saved_x = crtc->x;
    saved_y = crtc->y;

    crtc->mode = *mode;
    crtc->rotation = rotation;
    crtc->x = x;
    crtc->y = y;

    xengfx_mode_to_kmode(&xengfx_crtc->kmode, mode);

    if (!xengfx_crtc_apply(crtc))
    {
        crtc->mode = saved_mode;
        crtc->rotation = saved_rotation;
        crtc->x = saved_x;
        crtc->y = saved_y;
        return FALSE;
    }

    return TRUE;
}


static void
xengfx_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
    // Both Intel and modesetting do not implement this
    // We can check other drivers later when we have time
}


static void
xengfx_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;

    drmModeMoveCursor(drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id, x, y);
}


static void
xengfx_crtc_show_cursor(xf86CrtcPtr crtc)
{
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;
    uint32_t handle = xengfx_crtc->cursor_bo->handle;

    drmModeSetCursor(drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id, handle, 64, 64);
}


static void
xengfx_crtc_hide_cursor(xf86CrtcPtr crtc)
{
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;

    drmModeSetCursor(drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id, 0, 64, 64);
}


static void
xengfx_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
#if 0
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    uint32_t handle = xengfx_crtc->cursor_bo->handle; // assume cursor is already mapped
    uint32_t *ptr = (uint32_t *) xengfx_crtc->cursor_bo->ptr;
    int i, ret;

    for (i = 0; i < 64*64; ++i)
        ptr[i] = image[i];

    ret = drmModeSetCursor(xengfx_crtc->drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id,
                           handle, 64, 64);

    // if fail, fallback to SW cursor
    if (ret)
    {
        xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
        xf86CursorInfoPtr cursor_info = xf86_config->cursor_info;

        cursor_info->MaxWidth = cursor_info->MaxHeight = 0;
    }
#endif

    // We do not support HW cursor for the moment
}


static void*
xengfx_crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
    ScrnInfoPtr scrn = crtc->scrn;
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *mode = xengfx_crtc->drm_mode;
    int ret;

    xengfx_crtc->rotate_bo = xengfx_drm_create_bo(mode->fd, width, height, scrn->bitsPerPixel);
    if (!xengfx_crtc->rotate_bo)
    {
        xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR, "Couldn't allocate shadow memory for rotated CRTC.\n");
        return NULL;
    }

    ret = drmModeAddFB(mode->fd, width, height, scrn->depth, scrn->bitsPerPixel,
                       xengfx_crtc->rotate_bo->pitch, xengfx_crtc->rotate_bo->handle,
                       &xengfx_crtc->rotate_fb_id);
    if (ret)
    {
        ErrorF("failed to rotate fb.\n");
        xengfx_drm_destroy_bo(mode->fd, xengfx_crtc->rotate_bo);
        return NULL;
    }

    xengfx_crtc->rotate_pitch = xengfx_crtc->rotate_bo->pitch;
    return xengfx_crtc->rotate_bo;
}


static PixmapPtr
xengfx_crtc_shadow_create(xf86CrtcPtr crtc,  void *data, int width, int height)
{
    ScrnInfoPtr scrn = crtc->scrn;
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    PixmapPtr rotate_pixmap;

    if (!data)
    {
        data = xengfx_crtc_shadow_allocate(crtc, width, height);
        if (!data)
        {
            xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Couldn't allocate shadow memory for rotated CRTC.\n");
            return NULL;
        }
    }

    if (xengfx_crtc->rotate_bo == NULL)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Couldn't allocate shadow memory for rotated CRTC.\n");
        return NULL;
    }

    rotate_pixmap = GetScratchPixmapHeader(scrn->pScreen, width, height, scrn->depth,
                                           scrn->bitsPerPixel, xengfx_crtc->rotate_pitch, NULL);
    if (!rotate_pixmap)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Couldn't allocate shadow memory for rotated CRTC.\n");
        return NULL;
    }

    return rotate_pixmap;
}


static void
xengfx_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *mode = xengfx_crtc->drm_mode;

    if (rotate_pixmap)
    {
        FreeScratchPixmapHeader(rotate_pixmap);
    }

    if (data)
    {
        drmModeRmFB(mode->fd, xengfx_crtc->rotate_fb_id);
        xengfx_crtc->rotate_fb_id = 0;

        xengfx_drm_destroy_bo(mode->fd, xengfx_crtc->rotate_bo);
        xengfx_crtc->rotate_bo = NULL;
    }
}

static void
xengfx_crtc_gamma_set(xf86CrtcPtr crtc, uint16_t *red, uint16_t *green,
                      uint16_t *blue, int size)
{
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;

    drmModeCrtcSetGamma(drm_mode->fd, xengfx_crtc->mode_crtc->crtc_id,
                        size, red, green, blue);
}


static void
xengfx_crtc_destroy(xf86CrtcPtr crtc)
{
    struct xengfx_crtc *xengfx_crtc = crtc->driver_private;

    xengfx_crtc_hide_cursor(crtc);
    // Unmap cursor

    free(xengfx_crtc);
    crtc->driver_private = NULL;
}


static const xf86CrtcFuncsRec xengfx_crtc_funcs = {
    .dpms = xengfx_crtc_dpms,
    .set_mode_major = xengfx_crtc_set_mode_major,
    .set_cursor_colors = xengfx_crtc_set_cursor_colors,
    .set_cursor_position = xengfx_crtc_set_cursor_position,
    .show_cursor = xengfx_crtc_show_cursor,
    .hide_cursor = xengfx_crtc_hide_cursor,
    .load_cursor_argb = xengfx_crtc_load_cursor_argb,

    .shadow_create = xengfx_crtc_shadow_create,
    .shadow_allocate = xengfx_crtc_shadow_allocate,
    .shadow_destroy = xengfx_crtc_shadow_destroy,

    .gamma_set = xengfx_crtc_gamma_set,
    .destroy = xengfx_crtc_destroy
};


void
xengfx_crtc_init(ScrnInfoPtr scrn, struct xengfx_drm_mode *drm_mode, int num)
{
    xf86CrtcPtr crtc;
    struct xengfx_crtc *xengfx_crtc;

    crtc = xf86CrtcCreate(scrn, &xengfx_crtc_funcs);
    if (!crtc)
        return;

    xengfx_crtc = xnfcalloc(sizeof (struct xengfx_crtc), 1);
    xengfx_crtc->mode_crtc = drmModeGetCrtc(drm_mode->fd, drm_mode->mode_res->crtcs[num]);
    xengfx_crtc->drm_mode = drm_mode;
    crtc->driver_private = xengfx_crtc;
}

Bool
xengfx_crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    struct xengfx_crtc *xengfx_crtc = xf86_config->crtc[0]->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_crtc->drm_mode;
    struct xengfx_bo *old_front = NULL;
    Bool ret;
    ScreenPtr screen = screenInfo.screens[scrn->scrnIndex];
    PixmapPtr ppix = screen->GetScreenPixmap(screen);
    uint32_t old_fb_id;
    int i, pitch, old_width, old_height, old_pitch;
    int cpp = (scrn->bitsPerPixel + 7) / 8;
    void *new_pixels;

    if (scrn->virtualX == width && scrn->virtualY == height)
        return TRUE;

    old_width = scrn->virtualX;
    old_height = scrn->virtualY;
    old_pitch = drm_mode->front_bo->pitch;
    old_fb_id = drm_mode->fb_id;
    old_front = drm_mode->front_bo;

    drm_mode->front_bo = xengfx_drm_create_bo(drm_mode->fd, width, height, scrn->bitsPerPixel);
    if (!drm_mode->front_bo)
        goto fail;

    pitch = drm_mode->front_bo->pitch;

    scrn->virtualX = width;
    scrn->virtualY = height;
    scrn->displayWidth = pitch / cpp;

    ret = drmModeAddFB(drm_mode->fd, width, height, scrn->depth, scrn->bitsPerPixel,
                       pitch, drm_mode->front_bo->handle, &drm_mode->fb_id);
    if (ret)
        goto fail;

    new_pixels = xengfx_drm_map_front_bo(drm_mode);
    if (!new_pixels)
        goto fail;

    screen->ModifyPixmapHeader(ppix, width, height, -1, -1, pitch, new_pixels);

    for (i = 0; i < xf86_config->num_crtc; ++i)
    {
        xf86CrtcPtr crtc = xf86_config->crtc[i];

        if (!crtc->enabled)
            continue;

        xengfx_crtc_set_mode_major(crtc, &crtc->mode, crtc->rotation,
                                   crtc->x, crtc->y);
    }

    if (old_fb_id)
    {
        drmModeRmFB(drm_mode->fd, old_fb_id);
        xengfx_drm_destroy_bo(drm_mode->fd, old_front);
    }

    return TRUE;

fail:
    if (drm_mode->front_bo)
        xengfx_drm_destroy_bo(drm_mode->fd, drm_mode->front_bo);
    drm_mode->front_bo = old_front;
    scrn->virtualX = old_width;
    scrn->virtualY = old_height;
    scrn->displayWidth = old_pitch / cpp;
    drm_mode->fb_id = old_fb_id;
    return FALSE;
}

