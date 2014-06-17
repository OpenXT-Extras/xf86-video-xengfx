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

#ifndef XENGFX_DRM_H_
#define XENGFX_DRM_H_

// This is from xengfx_drm_driver kernel module
// Please make sure the definitions are the same

#define DRM_XENGFX_GEM_CREATE   0x0
#define DRM_XENGFX_GEM_MAP      0x1

#define DRM_IOCTL_XENGFX_GEM_CREATE     DRM_IOWR(DRM_COMMAND_BASE + DRM_XENGFX_GEM_CREATE, struct drm_xengfx_gem_create)
#define DRM_IOCTL_XENGFX_GEM_MAP        DRM_IOWR(DRM_COMMAND_BASE + DRM_XENGFX_GEM_MAP, struct drm_xengfx_gem_map)

struct drm_xengfx_gem_create
{
    // IN
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;

    // OUT
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};


struct drm_xengfx_gem_map
{
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

#endif /* XENGFX_DRM_H_ */
