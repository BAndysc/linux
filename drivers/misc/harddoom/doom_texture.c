#include "doom_texture.h"
#include "doom_internal.h"

#define MIN(x, y) ((x)<(y)?(x):(y))

///
/// FLAT TEXTURES
///

int create_flat_texture(struct doomdev* dev, struct flat_texture* texture, u8* data)
{
    texture->parent_device = dev;
    //@todo magic
    if (alloc_doom_page(dev, &texture->data, HARDDOOM_FLAT_SIZE, HARDDOOM_FLAT_SIZE))
        return -ENOMEM;

    memcpy(texture->data.memory, data, HARDDOOM_FLAT_SIZE);

    return 0;
}

static void free_flat_texture(struct doomdev* dev, struct flat_texture* texture)
{
    free_doom_page(dev, &texture->data);
}

static int flat_texture_close(struct inode *inode, struct file *filp)
{
    struct flat_texture* tex = filp->private_data;
    struct doomdev* dev = tex->parent_device;

    PASS_ERROR_IF_ANY(lock_device(dev));

    free_flat_texture(dev, tex);

    kfree(tex);

    unlock_device(dev);

    return 0;
}

///
/// COLOR MAPS
///

static void free_colormap(struct doomdev* dev, struct colormap* map)
{
    for (int i = 0; i < map->size; ++i)
        free_doom_page(dev, &map->maps[i]);

    kfree(map->maps);
}

int create_colormap(struct doomdev* dev, struct colormap* map, u8* data, int num)
{
    if (num < 1 || num > HARDDOOM_MAX_COLORMAP_ARRAY)
        return -EOVERFLOW;

    map->parent_device = dev;
    struct doompage* maps = kmalloc(sizeof(struct doompage) * num, GFP_KERNEL);

    if (IS_ERR_OR_NULL(maps))
        return -ENOMEM;

    map->maps = maps;
    map->size = 0;

    for (int i = 0; i < num; ++i) {

        if (alloc_doom_page(dev, maps+i, HARDDOOM_COLORMAP_SIZE, HARDDOOM_COLORMAP_SIZE))
        {
            goto err;
        }

        memcpy(map->maps[i].memory, data + i * HARDDOOM_COLORMAP_SIZE, HARDDOOM_COLORMAP_SIZE);

        map->size++;
    }

    return 0;

    err:
    free_colormap(dev, map);
    return -ENOMEM;
}

static int colormap_close(struct inode *inode, struct file *filp)
{
    struct colormap* map = filp->private_data;
    struct doomdev* dev = map->parent_device;

    PASS_ERROR_IF_ANY(lock_device(dev));

    free_colormap(dev, map);

    kfree(map);

    unlock_device(dev);

    return 0;
}

///
/// NORMAL TEXTURES
///

static void free_texture(struct doomdev* dev, struct texture* texture)
{
    free_pagetable(dev, &texture->table);
}

int create_texture(struct doomdev* dev, struct texture* texture, int size, int height, u8 __user* data)
{
    if (size > HARDDOOM_MAX_TEXTURE_SIZE || size < 1 || height > HARDDOOM_MAX_TEXTURE_HEIGHT)
        return -EOVERFLOW;

    texture->parent_device = dev;
    texture->size = size;
    texture->height = height;

    int pages = size / DOOM_PAGE_ALIGN;

    if ((size % DOOM_PAGE_ALIGN) > 0)
        pages += 1;

    int res = create_pagetable(dev, &texture->table, DOOM_PAGE_SIZE, DOOM_PAGE_ALIGN, pages);

    if (res)
        return res;

    // copying
    int written = 0;
    int page = 0;

    while (written < size) {
        if (copy_from_user(texture->table.pages[page].memory, data+written, MIN(DOOM_PAGE_SIZE, size - written))) {
            free_texture(dev, texture);
            return -EINVAL;
        }
        written +=  MIN(DOOM_PAGE_SIZE, size - written);
        page += 1;
    }
    if ((size % DOOM_PAGE_ALIGN) > 0) {
        memset(((u8*)texture->table.pages[page-1].memory) + (size%DOOM_PAGE_ALIGN), 0, DOOM_PAGE_ALIGN-size%DOOM_PAGE_ALIGN);
    }

    return 0;
}

static int texture_close(struct inode *inode, struct file *filp)
{
    struct texture* tex = filp->private_data;
    struct doomdev* dev = tex->parent_device;

    PASS_ERROR_IF_ANY(lock_device(dev));

    free_texture(dev, tex);

    kfree(tex);

    unlock_device(dev);

    return 0;
}


struct file_operations flat_texture_fops = {
    .owner =    THIS_MODULE,
    .release =    &flat_texture_close
};
struct file_operations texture_fops = {
    .owner =    THIS_MODULE,
    .release = &texture_close
};
struct file_operations coloramap_fops = {
    .owner =    THIS_MODULE,
    .release = &colormap_close
};
