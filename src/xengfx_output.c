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

#include <X11/Xatom.h>
#include <xf86Crtc.h>
#include <xf86DDC.h>


static Bool
xengfx_property_ignore(drmModePropertyPtr prop)
{
    if (!prop)
        return TRUE;

    // ignore blob prop
    if (prop->flags & DRM_MODE_PROP_BLOB)
        return TRUE;

    // ignore standard property ?
    if (!strcmp(prop->name, "EDID") ||
        !strcmp(prop->name, "DPMS"))
        return TRUE;

    return FALSE;
}


static void
xengfx_output_create_ranged_atom(xf86OutputPtr output, Atom *atom,
                                 const char *name, INT32 min, INT32 max,
                                 uint64_t value, Bool immutable)
{
    int err;
    INT32 atom_range[2];

    atom_range[0] = min;
    atom_range[1] = max;

    *atom = MakeAtom(name, strlen(name), TRUE);

    err = RRConfigureOutputProperty(output->randr_output, *atom,
                                    FALSE, TRUE, immutable, 2, atom_range);
    if (err != 0)
        xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
                   "RRConfigureOutputProperty error. %d\n", err);

    err = RRChangeOutputProperty(output->randr_output, *atom, XA_INTEGER, 32,
                                 PropModeReplace, 1, &value, FALSE, TRUE);
    if (err != 0)
        xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
                   "RRChangeOutputProperty error. %d\n", err);
}


static void
xengfx_output_create_resources(xf86OutputPtr output)
{
    struct xengfx_output *xengfx_output = output->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_output->mode;
    drmModeConnectorPtr mode_output = xengfx_output->mode_output;
    int i, j, err;

    xengfx_output->props = calloc(mode_output->count_props, sizeof (struct xengfx_property));
    if (!xengfx_output->props)
        return;

    xengfx_output->num_props = 0;
    for (i = j = 0; i < mode_output->count_props; ++i)
    {
        drmModePropertyPtr drm_mode_prop;

        drm_mode_prop = drmModeGetProperty(drm_mode->fd, mode_output->props[i]);
        if (xengfx_property_ignore(drm_mode_prop))
        {
            drmModeFreeProperty(drm_mode_prop);
            continue;
        }

        xengfx_output->props[j].mode_prop = drm_mode_prop;
        xengfx_output->props[j].value = mode_output->prop_values[i];
        ++j;
    }
    xengfx_output->num_props = j;

    for (i = 0; i < xengfx_output->num_props; ++i)
    {
        struct xengfx_property *p = &xengfx_output->props[i];
        drmModePropertyPtr drm_mode_prop = p->mode_prop;

        if (drm_mode_prop->flags & DRM_MODE_PROP_RANGE)
        {
            p->num_atoms = 1;
            p->atoms = calloc(p->num_atoms, sizeof (Atom));
            if (!p->atoms)
                continue;

            xengfx_output_create_ranged_atom(output, &p->atoms[0],
                                             drm_mode_prop->name,
                                             drm_mode_prop->values[0],
                                             drm_mode_prop->values[1],
                                             p->value,
                                             drm_mode_prop->flags & DRM_MODE_PROP_IMMUTABLE ?
                                             TRUE : FALSE);
        }
        else if (drm_mode_prop->flags & DRM_MODE_PROP_ENUM)
        {
            p->num_atoms = drm_mode_prop->count_enums + 1;
            p->atoms = calloc(p->num_atoms, sizeof (Atom));
            if (!p->atoms)
                continue;

            p->atoms[0] = MakeAtom(drm_mode_prop->name, strlen(drm_mode_prop->name), TRUE);
            for (j = 1; j <= drm_mode_prop->count_enums; ++j)
            {
                struct drm_mode_property_enum *e = &drm_mode_prop->enums[j - 1];
                p->atoms[j] = MakeAtom(e->name, strlen(e->name), TRUE);
            }

            err = RRConfigureOutputProperty(output->randr_output, p->atoms[0], FALSE, FALSE,
                                            drm_mode_prop->flags & DRM_MODE_PROP_IMMUTABLE ? TRUE : FALSE,
                                            p->num_atoms -1, (INT32 *) &p->atoms[1]);
            if (err != 0)
                xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
                           "RRConfigureOutputProperty error. %d\n", err);

            for (j = 0; j < drm_mode_prop->count_enums; ++j)
                if (drm_mode_prop->enums[j].value == p->value)
                    break;

            // there is always a matching value
            err = RRChangeOutputProperty(output->randr_output, p->atoms[0], XA_ATOM, 32,
                                         PropModeReplace, 1, &p->atoms[j + 1], FALSE, TRUE);
            if (err != 0)
                xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
                           "RRChangeOutputProperty error. %d\n", err);
        }
    }
}


static Bool
xengfx_output_set_property(xf86OutputPtr output, Atom property,
                           RRPropertyValuePtr value)
{
    struct xengfx_output *xengfx_output = output->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_output->mode;
    int i;

    for (i = 0; i < xengfx_output->num_props; ++i)
    {
        struct xengfx_property *p = &xengfx_output->props[i];

        if (p->atoms[0] == property)
            continue;

        if (p->mode_prop->flags & DRM_MODE_PROP_RANGE)
        {
            uint32_t val;

            if (value->type != XA_INTEGER || value->format != 32 || value->size != 1)
                return FALSE;
            val = *(uint32_t *)value->data;

            drmModeConnectorSetProperty(drm_mode->fd, xengfx_output->output_id,
                                        p->mode_prop->prop_id, (uint64_t) val);
            return TRUE;
        }
        else if (p->mode_prop->flags & DRM_MODE_PROP_ENUM)
        {
            Atom        atom;
            const char  *name;
            int         j;

            if (value->type != XA_INTEGER || value->format != 32 || value->size != 1)
                return FALSE;
            memcpy(&atom, value->data, 4);
            name = NameForAtom(atom);

            // search for matching name string, then set its value down
            for (j = 0; j < p->mode_prop->count_enums; ++j)
            {
                if (!strcmp(p->mode_prop->enums[j].name, name))
                {
                    drmModeConnectorSetProperty(drm_mode->fd, xengfx_output->output_id,
                                                p->mode_prop->prop_id, p->mode_prop->enums[j].value);
                    return TRUE;
                }
            }

            return FALSE;
        }
    }

    // No property found
    return TRUE;
}


static Bool
xengfx_output_get_property(xf86OutputPtr output, Atom property)
{
    return TRUE;
}


static void
xengfx_output_dpms(xf86OutputPtr output, int mode)
{
    // XXX : not really critical right now
}


static xf86OutputStatus
xengfx_output_detect(xf86OutputPtr output)
{
    struct xengfx_output *xengfx_output = output->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_output->mode;

    drmModeFreeConnector(xengfx_output->mode_output);
    xengfx_output->mode_output = drmModeGetConnector(drm_mode->fd, xengfx_output->output_id);

    switch (xengfx_output->mode_output->connection)
    {
        case DRM_MODE_CONNECTED:
            return XF86OutputStatusConnected;
        case DRM_MODE_DISCONNECTED:
            return XF86OutputStatusDisconnected;
        case DRM_MODE_UNKNOWNCONNECTION:
        default:
            return XF86OutputStatusUnknown;
    }
}


static Bool
xengfx_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
    return MODE_OK;
}


static void
xengfx_output_attach_edid(xf86OutputPtr output)
{
    struct xengfx_output *xengfx_output = output->driver_private;
    struct xengfx_drm_mode *drm_mode = xengfx_output->mode;
    drmModeConnectorPtr koutput = xengfx_output->mode_output;
    drmModePropertyBlobPtr edid_blob = NULL;
    xf86MonPtr mon = NULL;
    int i;

    // look for an EDID property
    for (i = 0; i < koutput->count_props; ++i)
    {
        drmModePropertyPtr props;

        props = drmModeGetProperty(drm_mode->fd, koutput->props[i]);
        if (!props)
            continue;
        if (!(props->flags & DRM_MODE_PROP_BLOB))
        {
            drmModeFreeProperty(props);
            continue;
        }

        if (!strcmp(props->name, "EDID"))
        {
            drmModeFreePropertyBlob(edid_blob);
            edid_blob = drmModeGetPropertyBlob(drm_mode->fd, koutput->prop_values[i]);
        }
        drmModeFreeProperty(props);
    }

    if (edid_blob)
    {
        mon = xf86InterpretEDID(output->scrn->scrnIndex, edid_blob->data);
        if (mon && edid_blob->length > 128)
            mon->flags |= MONITOR_EDID_COMPLETE_RAWDATA;
    }

    xf86OutputSetEDID(output, mon);
    // No free of edid_blob ?
}


static DisplayModePtr
xengfx_output_get_modes(xf86OutputPtr output)
{
    struct xengfx_output *xengfx_output = output->driver_private;
    drmModeConnectorPtr koutput = xengfx_output->mode_output;
    DisplayModePtr Modes = NULL;
    int i;

    xengfx_output_attach_edid(output);

    // modes should already be available
    for (i = 0; i < koutput->count_modes; ++i)
    {
        DisplayModePtr Mode;

        Mode = xnfalloc(sizeof (DisplayModeRec));
        if (Mode)
        {
            xengfx_mode_from_kmode(output->scrn, &koutput->modes[i], Mode);
            Modes = xf86ModesAdd(Modes, Mode);
        }
    }

    return Modes;
}


static void
xengfx_output_destroy(xf86OutputPtr output)
{
    struct xengfx_output *xengfx_output = output->driver_private;
    int i;

    for (i = 0; i < xengfx_output->num_props; ++i)
    {
        drmModeFreeProperty(xengfx_output->props[i].mode_prop);
        free(xengfx_output->props[i].atoms);
    }
    free(xengfx_output->props);

    drmModeFreeEncoder(xengfx_output->mode_encoder);
    drmModeFreeConnector(xengfx_output->mode_output);

    free(xengfx_output);
    output->driver_private = NULL;
}


static const xf86OutputFuncsRec xengfx_output_funcs = {
    .create_resources = xengfx_output_create_resources,
#ifdef RANDR_12_INTERFACE
    .set_property = xengfx_output_set_property,
    .get_property = xengfx_output_get_property,
#endif
    .dpms = xengfx_output_dpms,
    .detect = xengfx_output_detect,
    .mode_valid = xengfx_output_mode_valid,

    .get_modes = xengfx_output_get_modes,
    .destroy = xengfx_output_destroy,
};

static int subpixel_conv_table[7] = {
    0,
    SubPixelUnknown,
    SubPixelHorizontalRGB,
    SubPixelHorizontalBGR,
    SubPixelVerticalRGB,
    SubPixelVerticalBGR,
    SubPixelNone
};


void
xengfx_output_init(ScrnInfoPtr scrn, struct xengfx_drm_mode *mode, int num)
{
    struct xengfx_output *xengfx_output;
    drmModeConnectorPtr koutput;
    drmModeEncoderPtr kencoder;
    xf86OutputPtr output;
    static const char *output_name = "LVDS"; // We know it is a LVDS connector
    char name[32];

    koutput = drmModeGetConnector(mode->fd, mode->mode_res->connectors[num]);
    if (!koutput)
        return;

    // We know there is only one encoder in the kernel module
    if (koutput->count_encoders != 1)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "More than one encoder.\n");
        return;
    }

    kencoder = drmModeGetEncoder(mode->fd, koutput->encoders[0]);
    if (!kencoder)
        goto cleanup_connector;

    snprintf(name, 32, "%s-%d", output_name, koutput->connector_type_id);

    output = xf86OutputCreate(scrn, &xengfx_output_funcs, name);
    if (!output)
        goto cleanup_encoder;

    xengfx_output = calloc(sizeof (struct xengfx_output), 1);
    if (!xengfx_output)
        goto cleanup_output;

    xengfx_output->mode = mode;
    xengfx_output->output_id = mode->mode_res->connectors[num];
    xengfx_output->mode_output = koutput;
    xengfx_output->mode_encoder = kencoder;

    output->mm_width = koutput->mmWidth;
    output->mm_height = koutput->mmHeight;
    output->subpixel_order = subpixel_conv_table[koutput->subpixel];
    output->driver_private = xengfx_output;
    output->possible_crtcs = kencoder->possible_crtcs;
    output->possible_clones = 0;

    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;

    return;

cleanup_output:
    xf86OutputDestroy(output);
cleanup_encoder:
    drmModeFreeEncoder(kencoder);
cleanup_connector:
    drmModeFreeConnector(koutput);
}
