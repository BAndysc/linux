#include "doom_surface.h"
#include "doom_internal.h"
#include "doom_render.h"
#include "doom_texture.h"

#define MIN(x, y) ((x)<(y)?(x):(y))

int create_surface(struct doomdev* dev, struct surface* surf, int width, int height)
{
    if (width < HARDDOOM_MIN_SURFACE_WIDTH ||
            width > HARDDOOM_MAX_SURFACE_WIDTH ||
            (width % HARDDOOM_SURFACE_MOD) != 0 ||
            height < HARDDOOM_MIN_SURFACE_HEIGHT ||
            height > HARDDOOM_MAX_SURFACE_HEIGHT)
        return -EOVERFLOW;

    surf->parent_device = dev;
    surf->width = width;
    surf->height = height;

    int pages = width * height / DOOM_PAGE_ALIGN;

    if (((width * height) % DOOM_PAGE_ALIGN) > 0)
        pages += 1;

    int res = create_pagetable(dev, &surf->table, DOOM_PAGE_SIZE, DOOM_PAGE_ALIGN, pages);

    return res;
}

void free_surface(struct doomdev* dev, struct surface* surf) {
    free_pagetable(dev, &surf->table);
}


/// private

static int surface_open(struct inode *inode, struct file *filp)
{
    printk("surface Open\n");
    return 0;
}

static int surface_close(struct inode *inode, struct file *filp)
{
    struct surface* surface = filp->private_data;
    struct doomdev* dev = surface->parent_device;

    PASS_ERROR_IF_ANY(lock_device(dev));

    free_surface(dev, surface);

    kfree(surface);

    unlock_device(dev);

    return 0;
}


static loff_t surface_llseek(struct file *filp, loff_t off, int whence)
{
    struct surface* surf = filp->private_data;
    if (filp->f_op != &surface_fops)
        return -EINVAL;

    switch (whence) {
        case SEEK_SET:
            filp->f_pos = off;
        break;

        case SEEK_CUR:
            filp->f_pos += off;
        break;

        case SEEK_END:
            filp->f_pos = surf->width * surf->height + off;
        break;
    }

    return filp->f_pos;
}



static ssize_t surface_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct surface* surf = filp->private_data;
    struct doomdev* dev = surf->parent_device;

    PASS_ERROR_IF_ANY(lock_device(surf->parent_device));

    size_t total_read = 0;
    int page = *f_pos / DOOM_PAGE_SIZE;

    loff_t offset = *f_pos % DOOM_PAGE_SIZE;

    while (total_read < count) {
        if (surf->table.pages[page].mem_addr == 0)
            break;

        int to_read = MIN(DOOM_PAGE_SIZE - offset, count - total_read);

        if (copy_to_user(buf + total_read, (char*)surf->table.pages[page].memory + offset, to_read))
            break;

        offset = 0;
        page++;

        *f_pos = *f_pos + to_read;
        total_read += to_read;
    }

    unlock_device(dev);

    return total_read;
}

/// IOCTLs

static long ioctl_draw_lines(struct doomdev* dev, struct surface* surf, const void __user* arg_ptr)
{
    struct doomdev_surf_ioctl_draw_lines lines_arg;

    if (copy_from_user(&lines_arg, arg_ptr, sizeof(lines_arg))) {
        printk("error copying lines data");
        return -EINVAL; // @todo jaki blad?
    }

    return doom_draw_lines(surf, (const struct doomdev_line __user*) lines_arg.lines_ptr, lines_arg.lines_num);
}

static long ioctl_fill_rects(struct doomdev* dev, struct surface* surf, const void __user* arg_ptr)
{
    struct doomdev_surf_ioctl_fill_rects rects_arg;

    if (copy_from_user(&rects_arg, arg_ptr, sizeof(rects_arg))) {
        printk("error copying rect data");
        return -EINVAL; // @todo jaki blad?
    }

    return doom_fill_rects(surf, (const struct doomdev_fill_rect __user*) rects_arg.rects_ptr, rects_arg.rects_num);
}

static long ioctl_copy_rects(struct doomdev* dev, struct surface* surf, const void __user* arg_ptr)
{
    struct doomdev_surf_ioctl_copy_rects rects_arg;

    if (copy_from_user(&rects_arg, arg_ptr, sizeof(rects_arg))) {
        printk("error copying rect data");
        return -EINVAL; // @todo jaki blad?
    }

    struct fd f;
    f = fdget(rects_arg.surf_src_fd);
    if (!f.file)
        return -EINVAL;

    if (f.file->f_op != &surface_fops) {
        fdput(f);
        return -EINVAL;
    }
    struct surface* src = f.file->private_data;

    int result = doom_copy_rects(surf, src, (const struct doomdev_copy_rect __user*) rects_arg.rects_ptr, rects_arg.rects_num);

    fdput(f);

    return result;
}

static long ioctl_draw_columns(struct doomdev* dev, struct surface* surf, const void __user* arg_ptr)
{
    struct doomdev_surf_ioctl_draw_columns rects_arg;

    if (copy_from_user(&rects_arg, arg_ptr, sizeof(rects_arg))) {
        printk("error copying rect data");
        return -EINVAL; // @todo jaki blad?
    }

    return doom_draw_columns(surf, &rects_arg, (const struct doomdev_column __user*) rects_arg.columns_ptr, rects_arg.columns_num);
}

static long ioctl_draw_spans(struct doomdev* dev, struct surface* surf, const void __user* arg_ptr)
{
    struct doomdev_surf_ioctl_draw_spans rects_arg;

    if (copy_from_user(&rects_arg, arg_ptr, sizeof(rects_arg))) {
        printk("error copying rect data");
        return -EINVAL; // @todo jaki blad?
    }

    struct fd f;
    f = fdget(rects_arg.flat_fd);
    if (!f.file)
        return -EINVAL;

    if (f.file->f_op != &flat_texture_fops) {
        fdput(f);
        return -EINVAL;
    }

    struct flat_texture* tex = f.file->private_data;

    int result = doom_draw_spans(surf, &rects_arg, tex, (const struct doomdev_span __user*)  rects_arg.spans_ptr, rects_arg.spans_num);

    fdput(f);

    return result;
}

static long ioctl_draw_background(struct doomdev* dev, struct surface* surf, const void __user* arg_ptr)
{
    struct doomdev_surf_ioctl_draw_background params;

    if (copy_from_user(&params, (const void __user*)arg_ptr, sizeof(struct doomdev_surf_ioctl_draw_background))) {
        printk("error copying rect data");
        return -EINVAL; // @todo jaki blad?
    }

    struct fd f;
    f = fdget(params.flat_fd);
    if (!f.file)
        return -EINVAL;

    if (f.file->f_op != &flat_texture_fops) {
        fdput(f);
        return -EINVAL;
    }

    struct flat_texture* texture = f.file->private_data;

    int result = doom_draw_background(surf, texture);

    fdput(f);

    return result;
}

static long surface_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct surface* surf = filp->private_data;
    struct doomdev* dev = surf->parent_device;
    switch (cmd) {
        case DOOMDEV_SURF_IOCTL_DRAW_LINES:
            return ioctl_draw_lines(dev, surf, (const void __user*)arg);
        case DOOMDEV_SURF_IOCTL_COPY_RECTS:
            return ioctl_copy_rects(dev, surf, (const void __user*)arg);
        case DOOMDEV_SURF_IOCTL_FILL_RECTS:
            return ioctl_fill_rects(dev, surf, (const void __user*)arg);
        case DOOMDEV_SURF_IOCTL_DRAW_BACKGROUND:
            return ioctl_draw_background(dev, surf, (const void __user*)arg);
        case DOOMDEV_SURF_IOCTL_DRAW_COLUMNS:
            return ioctl_draw_columns(dev, surf, (const void __user*)arg);
        case DOOMDEV_SURF_IOCTL_DRAW_SPANS:
            return ioctl_draw_spans(dev, surf, (const void __user*)arg);
        default:
            printk("surface ioctl: %d\n", cmd);
            break;
    }
    return 0;
}

struct file_operations surface_fops = {
    .owner =    THIS_MODULE,
    .open =     &surface_open,
    .read =     &surface_read,
    .release =    &surface_close,
    .unlocked_ioctl =    &surface_ioctl,
    .compat_ioctl =    &surface_ioctl,
    .llseek = &surface_llseek
};
