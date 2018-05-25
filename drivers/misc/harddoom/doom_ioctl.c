#include "doom_internal.h"
#include "doom_ioctl.h"
#include "doom_surface.h"
#include "doom_texture.h"

long ioctl_create_surface(struct doomdev* dev, struct doomdev_ioctl_create_surface* args)
{
    struct surface* mem_for_surface = kmalloc(sizeof(struct surface), GFP_KERNEL);

    if (IS_ERR_OR_NULL(mem_for_surface))
        return -ENOMEM;

    if (create_surface(dev, mem_for_surface, args->width, args->height))
        goto error;

    int fd = anon_inode_getfd("/doom/surface", &surface_fops, mem_for_surface, O_CLOEXEC | O_RDONLY);

    if (fd < 0)
        goto error;

    struct fd surface_file = fdget(fd);

    if (IS_ERR_OR_NULL(surface_file.file))
        goto error;

    surface_file.file->f_mode |= FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;

    fdput(surface_file);

    return fd;

error:
    kfree(mem_for_surface);
    return -ENOMEM;
}

long ioctl_create_flat(struct doomdev* dev, struct doomdev_ioctl_create_flat* args)
{
    u8* data = kmalloc(HARDDOOM_FLAT_SIZE, GFP_KERNEL);

    if (IS_ERR_OR_NULL(data))
        goto error;

    if (copy_from_user(data, (const void __user *) args->data_ptr, HARDDOOM_FLAT_SIZE))
        goto copy_from_user_error;

    struct flat_texture* mem_for_tex = kmalloc(sizeof(struct flat_texture), GFP_KERNEL);

    if (IS_ERR_OR_NULL(mem_for_tex))
        goto tex_alloc_error;

    if (create_flat_texture(dev, mem_for_tex, data))
        goto create_flat_error;

    int fd = anon_inode_getfd("/doom/texture/flat", &flat_texture_fops, mem_for_tex, O_CLOEXEC | O_RDONLY);

    if (fd < 0)
        goto get_file_error;

    kfree(data);

    return fd;

    get_file_error:
    create_flat_error:
    kfree(mem_for_tex);

    tex_alloc_error:
    copy_from_user_error:
    kfree(data);

    error:
    return -ENOMEM;
}

long ioctl_create_texture(struct doomdev* dev, struct doomdev_ioctl_create_texture* args)
{
    struct texture* mem_for_tex = kmalloc(sizeof(struct texture), GFP_KERNEL);

    if (IS_ERR_OR_NULL(mem_for_tex))
        return -ENOMEM;

    if (create_texture(dev, mem_for_tex, args->size, args->height, (u8 __user*) args->data_ptr))
        goto err;

    int fd = anon_inode_getfd("/doom/texture", &texture_fops, mem_for_tex, O_CLOEXEC | O_RDONLY);

    if (fd < 0)
        goto err;

    return fd;

err:
    kfree(mem_for_tex);
    return -EINVAL;
}

long ioctl_create_colormap(struct doomdev* dev, struct doomdev_ioctl_create_colormaps* args)
{
    u8* data = kmalloc(HARDDOOM_COLORMAP_SIZE * args->num, GFP_KERNEL);

    if (IS_ERR_OR_NULL(data))
        return -ENOMEM;

    if (copy_from_user(data, (const void __user *) args->data_ptr, HARDDOOM_COLORMAP_SIZE * args->num)) {
        kfree(data);
        return -EINVAL;
    }

    struct colormap* mem_for_tex = kmalloc(sizeof(struct colormap), GFP_KERNEL);

    if (IS_ERR_OR_NULL(mem_for_tex)) {
        kfree(data);
        return -ENOMEM;
    }

    if (create_colormap(dev, mem_for_tex, data, args->num))
        goto err;

    int fd = anon_inode_getfd("/doom/colormap", &coloramap_fops, mem_for_tex, O_CLOEXEC | O_RDONLY);

    if (fd < 0)
        goto err;

    kfree(data);

    return fd;

err:
    kfree(mem_for_tex);
    kfree(data);
    return -EINVAL;
}
