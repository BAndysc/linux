#ifndef DOOM_TEXTURE_H
#define DOOM_TEXTURE_H

#include <linux/types.h>
#include "doom_pagetable.h"
#include "harddoom.h"
#include "doomdev.h"

struct texture
{
    struct doom_page_table table;

    struct doomdev* parent_device;
    int size;
    int height;
};

struct flat_texture
{
    struct doompage data;
    struct doomdev* parent_device;
};

struct colormap
{
    struct doompage* maps;
    struct doomdev* parent_device;

    int size;
};

/**
 * @brief Creates texture with given dimnension and content
 * @param dev Device
 * @param texture Pointer to fill
 * @param size Total texture size in bytes (max 4MiB)
 * @param height Height of the texture (1-1023) or 0 if texture is not repeated
 * @param data pointer from USERSPACE to texture data
 * @return 0 on success, -ENOMEM when no memory, -EOVERFLOW when dimentions out of range
 */
int create_texture(struct doomdev* dev, struct texture* texture, int size, int height, u8 __user* data);

/**
 * @brief Creates color map with given colors
 * @param dev Device
 * @param map Pointer to fill
 * @param data Array with colors for map
 * @param num Number of maps (1-255)
 * @return 0 on success, -ENOMEM when no memory, -EOVERFLOW when dimentions out of range
 */
int create_colormap(struct doomdev* dev, struct colormap* map, u8* data, int num);

/**
 * @brief Creates flat texture
 * @param dev Device
 * @param texture Pointer to fill
 * @param data Pointer to texture data
 * @return 0 on success, -ENOMEM when no memory
 */
int create_flat_texture(struct doomdev* dev, struct flat_texture* texture, u8* data);

extern struct file_operations flat_texture_fops;
extern struct file_operations texture_fops;
extern struct file_operations coloramap_fops;

#define HARDDOOM_MAX_TEXTURE_SIZE (1<<22)
#define HARDDOOM_MAX_TEXTURE_HEIGHT 1023

#define HARDDOOM_MAX_COLORMAP_ARRAY 0x100
#define HARDDOOM_COLORMAP_SIZE 0x100

#endif // DOOM_TEXTURE_H
