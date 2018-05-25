#ifndef DOOM_IOCTL_H
#define DOOM_IOCTL_H

#include "doomdev.h"

struct doomdev;

long ioctl_create_surface(struct doomdev* dev, struct doomdev_ioctl_create_surface* args);

long ioctl_create_flat(struct doomdev* dev, struct doomdev_ioctl_create_flat* args);

long ioctl_create_texture(struct doomdev* dev, struct doomdev_ioctl_create_texture* args);

long ioctl_create_colormap(struct doomdev* dev, struct doomdev_ioctl_create_colormaps* args);

#endif // DOOM_IOCTL_H
