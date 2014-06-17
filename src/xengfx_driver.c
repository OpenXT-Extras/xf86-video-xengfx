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

#include <X11/extensions/randr.h>
#include <micmap.h>
#include <fb.h>

static SymTabRec xengfx_chipsets[] =
{
    {XENGFX_DEVICE_ID,  "xengfx"},
    {-1,                NULL}
};

static const struct pci_id_match xengfx_device_match[] =
{
    {
        XENGFX_VENDOR_ID, XENGFX_DEVICE_ID, PCI_MATCH_ANY, PCI_MATCH_ANY,
        0, 0, 0
    }
};

static const OptionInfoRec xengfx_options[] =
{
    {-1, NULL, OPTV_NONE, {0}, FALSE}
};

static Bool
xengfx_get_rec(ScrnInfoPtr scrn)
{
    if (scrn->driverPrivate)
        return TRUE;

    scrn->driverPrivate = xnfcalloc(sizeof (struct xengfx_private), 1);

    return scrn->driverPrivate != NULL;
}

static int
xengfx_open_drm_master(ScrnInfoPtr scrn)
{
    struct xengfx_private *xengfx = to_xengfx_private(scrn);
    struct pci_device *pci = xengfx->PciInfo;
    char busid[20];
    int fd;

    snprintf(busid, sizeof (busid), "pci:%04x:%02x:%02x.%d",
             pci->domain, pci->bus, pci->dev, pci->func);

    fd = drmOpen("xengfx", busid);
    if (fd  < 0)
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                    "[drm] Failed to open DRM device for %s : %s\n",
                    busid, strerror(errno));

    return fd;
}

static void
xengfx_identify(int flags)
{
    xf86PrintChipsets(XENGFX_NAME,
                      "Driver for Xen GFX Emulated Graphics Chipsets",
                      xengfx_chipsets);
}


static const OptionInfoRec*
xengfx_available_options(int chipid, int busid)
{
    return xengfx_options;
}

static Bool
xengfx_pre_init(ScrnInfoPtr scrn, int flags)
{
    EntityInfoPtr pEnt;
    struct xengfx_private *xengfx;
    rgb initial_weight = { 0, 0, 0 };
    Gamma zeros = { 0.0, 0.0, 0.0 };

    if (scrn->numEntities != 1)
        return FALSE;

    pEnt = xf86GetEntityInfo(scrn->entityList[0]);

    if (flags & PROBE_DETECT) // XXX
        return FALSE;

    // Allocate driverPrivate
    if (!xengfx_get_rec(scrn))
        return FALSE;

    xengfx = to_xengfx_private(scrn);
    xengfx->pEnt = pEnt;

    scrn->displayWidth = 640;         //default it : will be overridden by xengfx_screen_init

    if (xengfx->pEnt->location.type != BUS_PCI)
        return FALSE;

    xengfx->PciInfo = xf86GetPciInfoForEntity(xengfx->pEnt->index);

    scrn->monitor = scrn->confScreen->monitor;
    scrn->progClock = TRUE;
    scrn->rgbBits = 8;

    if (!xf86SetDepthBpp(scrn, 0, 0, 0,
                         Support32bppFb | SupportConvert24to32 | PreferConvert24to32))
        return FALSE;

    switch (scrn->depth)
    {
        case 8:
        case 16:
        case 24:
        case 30:
            break;
        default:
            xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                       "Given depth (%d) is not supported by the driver\n",
                       scrn->depth);
            return FALSE;
    }
    xf86PrintDepthBpp(scrn);

    // Process the options
    xf86CollectOptions(scrn, NULL);
    if (!(xengfx->Options = malloc(sizeof (xengfx_options))))
        return FALSE;
    memcpy(xengfx->Options, xengfx_options, sizeof (xengfx_options));
    xf86ProcessOptions(scrn->scrnIndex, scrn->options, xengfx->Options);

    xengfx->fd = xengfx_open_drm_master(scrn);
    if (xengfx->fd < 0)
        return FALSE;

    if (!xf86SetWeight(scrn, initial_weight, initial_weight))
        return FALSE;
    if (!xf86SetDefaultVisual(scrn, -1))
        return FALSE;

    if (!xengfx_drm_pre_init(scrn, &xengfx->mode, scrn->bitsPerPixel / 8))
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "KMS setup failed\n");
        return FALSE;
    }

    if (!xf86SetGamma(scrn, zeros))
        return FALSE;

    if (scrn->modes == NULL)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "No modes.\n");
        return FALSE;
    }
    scrn->currentMode = scrn->modes;

    // Set display resolution
    xf86SetDpi(scrn, 0, 0);

    if (!xf86LoadSubModule(scrn, "fb"))
        return FALSE;

    return TRUE;
}


static Bool
xengfx_create_screen_resources(ScreenPtr screen)
{
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    struct xengfx_private *xengfx = to_xengfx_private(scrn);
    PixmapPtr rootPixmap;
    Bool ret;
    void *pixels;

    screen->CreateScreenResources = xengfx->CreateScreenResources;
    ret = screen->CreateScreenResources(screen);
    screen->CreateScreenResources = xengfx_create_screen_resources;

    if (!xengfx_drm_set_desired_modes(scrn, &xengfx->mode))
        return FALSE;

    pixels = xengfx_drm_map_front_bo(&xengfx->mode);
    if (!pixels)
        return FALSE;

    rootPixmap = screen->GetScreenPixmap(screen);
    if (!screen->ModifyPixmapHeader(rootPixmap, -1, -1, -1, -1, -1, pixels))
        return FALSE;

    xengfx->damage = DamageCreate(NULL, NULL, DamageReportNone, TRUE,
                                  screen, rootPixmap);
    if (!xengfx->damage)
        return FALSE;

    DamageRegister(&rootPixmap->drawable, xengfx->damage);
    return TRUE;
}


static void
xengfx_leave_vt(int scrnIndex, int flags)
{
    // XXX: Naive implementation
    ScrnInfoPtr scrn = xf86Screens[scrnIndex];

    scrn->vtSema = FALSE;
}


static Bool
xengfx_close_screen(int scrnIndex, ScreenPtr screen)
{
    ScrnInfoPtr scrn = xf86Screens[scrnIndex];
    struct xengfx_private *xengfx = to_xengfx_private(scrn);

    if (xengfx->damage)
    {
        DamageUnregister(&screen->GetScreenPixmap(screen)->drawable, xengfx->damage);
        DamageDestroy(xengfx->damage);
        xengfx->damage = NULL;
    }

    if (scrn->vtSema)
        xengfx_leave_vt(scrnIndex, 0);

    // XXX: Free GEM objects here

    screen->CreateScreenResources = xengfx->CreateScreenResources;
    xengfx->BlockHandler = xengfx->BlockHandler;

    drmDropMaster(xengfx->fd);

    screen->CloseScreen = xengfx->CloseScreen;
    return (*screen->CloseScreen) (scrnIndex, screen);
}


static void
xengfx_block_handler(int i, pointer blockData, pointer timeout, pointer readMask)
{
    ScreenPtr screen = screenInfo.screens[i];
    struct xengfx_private *xengfx = to_xengfx_private(xf86Screens[screen->myNum]);

    xengfx->BlockHandler(i, blockData, timeout, readMask);
}


static Bool
xengfx_enter_vt(int scrnIndex, int flags)
{
    ScrnInfoPtr scrn = xf86Screens[scrnIndex];
    struct xengfx_private *xengfx = to_xengfx_private(scrn);

    scrn->vtSema = TRUE;

    return xengfx_drm_set_desired_modes(scrn, &xengfx->mode);
}


static Bool
xengfx_screen_init(int scrnIndex,
                   ScreenPtr screen,
                   int argc,
                   char **argv)
{
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    struct xengfx_private *xengfx = to_xengfx_private(scrn);
    VisualPtr visual;
    int ret;

    scrn->pScreen = screen;
    ret = drmSetMaster(xengfx->fd);
    if (ret)
    {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Unable to set master\n");
        return FALSE;
    }

    // XXX Hardware dependent
    scrn->displayWidth = scrn->virtualX;
    if (!xengfx_drm_create_initial_bos(scrn, &xengfx->mode))
        return FALSE;

    miClearVisualTypes();
    if (!miSetVisualTypes(scrn->depth, miGetDefaultVisualMask(scrn->depth),
                          scrn->rgbBits, scrn->defaultVisual))
        return FALSE;
    if (!miSetPixmapDepths())
        return FALSE;

    scrn->memPhysBase = 0;
    scrn->fbOffset = 0;
    if (!fbScreenInit(screen, NULL, scrn->virtualX, scrn->virtualY,
                      scrn->xDpi, scrn->yDpi, scrn->displayWidth, scrn->bitsPerPixel))
        return FALSE;

    if (scrn->bitsPerPixel > 8)
    {
        visual = screen->visuals + screen->numVisuals;
        while (--visual >= screen->visuals)
        {
            if ((visual->class | DynamicClass) == DirectColor)
            {
                visual->offsetRed = scrn->offset.red;
                visual->offsetGreen = scrn->offset.green;
                visual->offsetBlue = scrn->offset.blue;
                visual->redMask = scrn->mask.red;
                visual->greenMask = scrn->mask.green;
                visual->blueMask = scrn->mask.blue;
            }
        }
    }

    fbPictureInit(screen, NULL, 0);

    xengfx->CreateScreenResources = screen->CreateScreenResources;
    screen->CreateScreenResources = xengfx_create_screen_resources;

    xf86SetBlackWhitePixels(screen);

    miInitializeBackingStore(screen);
    xf86SetBackingStore(screen);
    xf86SetSilkenMouse(screen);
    miDCInitialize(screen, xf86GetPointerScreenFuncs());

    // No xf86_cursors_init. we stay on SW cursor

    // Must force it before EnterVT, so we are in control of VT and
    // later memory should be bound when allocation e.g rotate_men
    scrn->vtSema = TRUE;

    screen->SaveScreen = xf86SaveScreen;

    xengfx->CloseScreen = screen->CloseScreen;
    screen->CloseScreen = xengfx_close_screen;

    xengfx->BlockHandler = screen->BlockHandler;
    screen->BlockHandler = xengfx_block_handler;

    // Intel also overrides a few other ones but I doubt we need it

    if (!xf86CrtcScreenInit(screen))
        return FALSE;

    if (!miCreateDefColormap(screen))
        return FALSE;

    xf86DPMSInit(screen, xf86DPMSSet, 0);

    if (serverGeneration == 1)
        xf86ShowUnusedOptions(scrn->scrnIndex, scrn->options);

    return xengfx_enter_vt(scrnIndex, 1);
}


static Bool
xengfx_switch_mode(int scrnIndex, DisplayModePtr mode, int flags)
{
    return xf86SetSingleMode(xf86Screens[scrnIndex], mode, RR_Rotate_0);
}


static void
xengfx_adjust_frame(int scrnIndex, int x, int y, int flags)
{
    // XXX : do we need to implement that ?
    // intel does not, but modesetting does
}


static void
xengfx_free_screen(int scrnIndex, int flags)
{
}


static ModeStatus
xengfx_valid_mode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    return MODE_OK;
}


static Bool
xengfx_pci_probe(DriverPtr              driver,
                 int                    entity_num,
                 struct pci_device      *device,
                 intptr_t               match_data)
{
    ScrnInfoPtr scrn = NULL;

    scrn = xf86ConfigPciEntity(NULL, 0, entity_num, NULL,
                               NULL, NULL, NULL, NULL, NULL);

    if (scrn)
    {
        scrn->driverVersion = XENGFX_VERSION;
        scrn->driverName = XENGFX_DRIVER_NAME;
        scrn->name = XENGFX_NAME;
        scrn->Probe = NULL;

        scrn->PreInit = xengfx_pre_init;
        scrn->ScreenInit = xengfx_screen_init;
        scrn->SwitchMode = xengfx_switch_mode;
        scrn->AdjustFrame = xengfx_adjust_frame;
        scrn->EnterVT = xengfx_enter_vt;
        scrn->LeaveVT = xengfx_leave_vt;
        scrn->FreeScreen = xengfx_free_screen;
        scrn->ValidMode = xengfx_valid_mode;

        // XXX
        scrn->PMEvent = NULL;
    }

    return scrn != NULL;
}

static MODULESETUPPROTO(xengfx_setup);

static DriverRec xengfx = {
    XENGFX_VERSION,
    XENGFX_DRIVER_NAME,
    xengfx_identify,
    NULL,
    xengfx_available_options,
    NULL,
    0,
    NULL,
    xengfx_device_match,
    xengfx_pci_probe
};

static pointer
xengfx_setup(pointer module,
             pointer opts,
             int *errmaj,
             int *errmin)
{
    static Bool setupDone = 0;

    // this module should be loaded only once but check to be sure
    if (!setupDone)
    {
        setupDone = 1;
        xf86AddDriver(&xengfx, module, HaveDriverFuncs);

        // the return value must be non-NULL on success even though there
        // is no TearDownProc
        return (pointer) 1;
    }
    else
    {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;
        return NULL;
    }
}

static XF86ModuleVersionInfo xengfx_version = {
    "xengfx",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    XENGFX_VERSION_MAJOR,
    XENGFX_VERSION_MINOR,
    XENGFX_VERSION_PATCH,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    { 0, 0, 0, 0 }
};

_X_EXPORT XF86ModuleData xengfxModuleData = { &xengfx_version, xengfx_setup, NULL };
