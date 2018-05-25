#ifndef DOOM_SURFACE_H
#define DOOM_SURFACE_H

#include <linux/types.h>
#include "doom_pagetable.h"
#include "harddoom.h"
#include "doomdev.h"

struct surface
{
    struct doom_page_table table;

    struct doomdev* parent_device;
    int width;
    int height;

    int is_diry;
};

extern struct file_operations surface_fops;

/**
 * @brief Creates surface
 * @param dev Device
 * @param surf Pointer to intialize
 * @param width Surface width: 64-2048 (must be multiple of 64)
 * @param height Surface height: 1-2048
 * @return 0 on success, -EOVERFLOW if dimensions out of range, -ENOMEM if no memory
 */
int create_surface(struct doomdev* dev, struct surface* surf, int width, int height);

/**
 * @brief Frees surface created by `create_surface`
 */
void free_surface(struct doomdev* dev, struct surface* surf);

#define HARDDOOM_MIN_SURFACE_WIDTH 64
#define HARDDOOM_MAX_SURFACE_WIDTH 2048
#define HARDDOOM_SURFACE_MOD 64

#define HARDDOOM_MIN_SURFACE_HEIGHT 1
#define HARDDOOM_MAX_SURFACE_HEIGHT 2048

#endif // DOOM_SURFACE_H
