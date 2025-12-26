#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

int main(void)
{
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open /dev/dri/card0");
        return 1;
    }

    /* Become DRM master */
    if (drmSetMaster(fd) < 0) {
        perror("drmSetMaster");
        /* continue anyway */
    }

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        perror("drmModeGetResources");
        return 1;
    }

    /* ---- Find connected connector ---- */
    drmModeConnector *conn = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (conn &&
            conn->connection == DRM_MODE_CONNECTED &&
            conn->count_modes > 0)
            break;

        drmModeFreeConnector(conn);
        conn = NULL;
    }

    if (!conn) {
        fprintf(stderr, "No connected display found\n");
        return 1;
    }

    drmModeModeInfo mode = conn->modes[0];

    /* ---- Find encoder ---- */
    drmModeEncoder *enc = NULL;

    if (conn->encoder_id)
        enc = drmModeGetEncoder(fd, conn->encoder_id);

    if (!enc) {
        for (int i = 0; i < conn->count_encoders; i++) {
            enc = drmModeGetEncoder(fd, conn->encoders[i]);
            if (enc)
                break;
        }
    }

    if (!enc) {
        fprintf(stderr, "No encoder found\n");
        return 1;
    }

    /* ---- Find compatible CRTC ---- */
    uint32_t crtc_id = 0;
    for (int i = 0; i < res->count_crtcs; i++) {
        if (enc->possible_crtcs & (1 << i)) {
            crtc_id = res->crtcs[i];
            break;
        }
    }

    if (!crtc_id) {
        fprintf(stderr, "No compatible CRTC found\n");
        return 1;
    }

    /* ---- Create dumb buffer ---- */
    struct drm_mode_create_dumb create = {
        .width  = mode.hdisplay,
        .height = mode.vdisplay,
        .bpp    = 32,
    };

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        perror("DRM_IOCTL_MODE_CREATE_DUMB");
        return 1;
    }

    uint32_t fb;
    if (drmModeAddFB(fd,
                     mode.hdisplay,
                     mode.vdisplay,
                     24, 32,
                     create.pitch,
                     create.handle,
                     &fb)) {
        perror("drmModeAddFB");
        return 1;
    }

    /* ---- Map dumb buffer ---- */
    struct drm_mode_map_dumb map = {
        .handle = create.handle
    };

    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map)) {
        perror("DRM_IOCTL_MODE_MAP_DUMB");
        return 1;
    }

    uint32_t *buf = mmap(NULL,
                         create.size,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         fd,
                         map.offset);

    if (buf == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    /* ---- Paint solid blue (XRGB8888) ---- */
    uint32_t stride = create.pitch / 4;
    for (uint32_t y = 0; y < mode.vdisplay; y++) {
        for (uint32_t x = 0; x < mode.hdisplay; x++) {
            buf[y * stride + x] = 0x000000FF;
        }
    }

    /* ---- Modeset ---- */
    if (drmModeSetCrtc(fd,
                       crtc_id,
                       fb,
                       0, 0,
                       &conn->connector_id,
                       1,
                       &mode)) {
        perror("drmModeSetCrtc");
        return 1;
    }

    write(1, "DRM OK: screen lit\n", 19);

    /* Keep screen alive */
    for (;;)
        pause();
}
