#include <linux/file.h>
#include <linux/fs.h>
#include "doom_surface.h"
#include "doom_texture.h"
#include "doom_queue.h"
#include "doom_render.h"

#define SIZE_SET_SURFACE 2
#define SIZE_SET_SRC_SURFACE 1
#define SIZE_SET_TEXTURE 2
#define SIZE_SET_COLORMAP 1
#define SIZE_DRAW_LINE 4
#define SIZE_SET_PARAMS 1
#define SIZE_SET_FLAT_TEXTURE 1
#define SIZE_DRAW_COLUMN_HEADER (SIZE_SET_PARAMS + SIZE_SET_TEXTURE + SIZE_SET_COLORMAP)
#define SIZE_DRAW_COLUMN (5 + SIZE_SET_COLORMAP)
#define SIZE_DRAW_SPAN (7 + SIZE_SET_COLORMAP)
#define SIZE_DRAW_SPAN_HEADER (SIZE_SET_SURFACE + SIZE_SET_FLAT_TEXTURE + SIZE_SET_COLORMAP)
#define SIZE_INTERLOCK 1
#define SIZE_COPY_RECT (3)
#define SIZE_COPY_RECT_HEADER (SIZE_SET_SURFACE + SIZE_SET_SRC_SURFACE + SIZE_INTERLOCK)
#define SIZE_FILL_RECT 3
#define SIZE_FILL_RECT_HEADER SIZE_SET_SURFACE
#define SIZE_DRAW_BACKGROUND (SIZE_SET_SURFACE + SIZE_SET_FLAT_TEXTURE + 1)


static int doom_set_dst_surf_new(struct surface* surface, struct doom_batch* batch)
{
    ENSURE_FREE_SLOTS(batch, SIZE_SET_SURFACE, -EOVERFLOW);

    push_doom_batch(batch, HARDDOOM_CMD_SURF_DIMS(surface->width, surface->height));
    push_doom_batch(batch, HARDDOOM_CMD_SURF_DST_PT(surface->table.table.dma));

    return 0;
}

static int doom_set_src_surf_new(struct surface* surface, struct doom_batch* batch)
{
    ENSURE_FREE_SLOTS(batch, SIZE_SET_SRC_SURFACE, -EOVERFLOW);

    push_doom_batch(batch, HARDDOOM_CMD_SURF_SRC_PT(surface->table.table.dma));
    // src pt is only used in copy rects, which sets dst pt and size always

    return 0;
}

static int doom_set_texture_new(int fd, struct doom_batch* batch)
{
    ENSURE_FREE_SLOTS(batch, SIZE_SET_TEXTURE, -EOVERFLOW);

    struct fd f;
    f = fdget(fd);
    if (!f.file)
        return -EINVAL;

    if (f.file->f_op != &texture_fops) {
        fdput(f);
        return -EINVAL;
    }

    struct texture* tex = f.file->private_data;

    push_doom_batch(batch, HARDDOOM_CMD_TEXTURE_DIMS(tex->size, tex->height));
    push_doom_batch(batch, HARDDOOM_CMD_TEXTURE_PT(tex->table.table.dma));

    fdput(f);

    return 0;
}

static int doom_set_flat_texture(struct doom_batch* batch, struct flat_texture* texture)
{
    push_doom_batch(batch, HARDDOOM_CMD_FLAT_ADDR(texture->data.dma));

    return 0;
}

static int doom_set_colormap_new(struct doom_batch* batch, int fd, int idx, int translation)
{
    struct fd f;
    f = fdget(fd);
    if (!f.file)
        return -EINVAL;

    if (f.file->f_op != &coloramap_fops) {
        fdput(f);
        return -EINVAL;
    }

    struct colormap* map = f.file->private_data;

    if (idx < 0 || idx >= map->size)
        return -EINVAL;

    if (translation) {
        push_doom_batch(batch, HARDDOOM_CMD_TRANSLATION_ADDR(map->maps[idx].dma));
    }
    else {
        push_doom_batch(batch, HARDDOOM_CMD_COLORMAP_ADDR(map->maps[idx].dma));
    }

    fdput(f);

    return 0;
}

static int doom_draw_line(struct surface* surface, struct doom_batch* batch, const struct doomdev_line __user* user_data)
{
    ENSURE_FREE_SLOTS(batch, SIZE_DRAW_LINE, -EOVERFLOW);

    struct doomdev_line data;
    if (copy_from_user(&data, user_data, sizeof(struct doomdev_line)))
        return -EINVAL;

    if (data.pos_a_x > surface->width ||
            data.pos_b_x > surface->width ||
            data.pos_a_y > surface->height ||
            data.pos_b_y > surface->height)
        return -EINVAL;

    push_doom_batch(batch, HARDDOOM_CMD_FILL_COLOR(data.color));
    push_doom_batch(batch, HARDDOOM_CMD_XY_A(data.pos_a_x, data.pos_a_y));
    push_doom_batch(batch, HARDDOOM_CMD_XY_B(data.pos_b_x, data.pos_b_y));
    push_doom_batch(batch, HARDDOOM_CMD_DRAW_LINE);

    return 0;
}
static int doom_fill_rect(struct surface* surface, struct doom_batch* batch, const struct doomdev_fill_rect __user* user_data)
{
    ENSURE_FREE_SLOTS(batch, SIZE_FILL_RECT, -EOVERFLOW);

    struct doomdev_fill_rect data;
    if (copy_from_user(&data, user_data, sizeof(struct doomdev_fill_rect)))
        return -EINVAL;

    if (data.pos_dst_x + data.width > surface->width ||
            data.pos_dst_y + data.height > surface->height) {
        printk(KERN_NOTICE "FILL_RECT: rect out of range");
        return -EINVAL;
    }

    push_doom_batch(batch, HARDDOOM_CMD_FILL_COLOR(data.color));

    push_doom_batch(batch, HARDDOOM_CMD_XY_A(data.pos_dst_x, data.pos_dst_y));

    push_doom_batch(batch, HARDDOOM_CMD_FILL_RECT(data.width, data.height));

    return 0;
}

static int doom_copy_rect(struct surface* surface, struct surface* src, struct doom_batch* batch, const struct doomdev_copy_rect __user* user_data)
{
    ENSURE_FREE_SLOTS(batch, SIZE_COPY_RECT, -EOVERFLOW);

    struct doomdev_copy_rect data;
    if (copy_from_user(&data, user_data, sizeof(struct doomdev_copy_rect)))
        return -EINVAL;

    if (data.pos_src_x + data.width > src->width ||
            data.pos_dst_x + data.width > surface->width ||
            data.pos_src_y + data.height > src->height ||
            data.pos_dst_y + data.height > surface->height) {
        printk(KERN_NOTICE "COPY_RECT: rect out of range");
        return -EINVAL;
    }

    push_doom_batch(batch, HARDDOOM_CMD_XY_A(data.pos_dst_x, data.pos_dst_y));
    push_doom_batch(batch, HARDDOOM_CMD_XY_B(data.pos_src_x, data.pos_src_y));

    push_doom_batch(batch, HARDDOOM_CMD_COPY_RECT(data.width, data.height));

    return 0;
}

static int doom_draw_column(struct doom_batch* batch, struct surface* surface, struct doomdev_surf_ioctl_draw_columns* info, const struct doomdev_column __user* user_data)
{
    ENSURE_FREE_SLOTS(batch, SIZE_DRAW_COLUMN, -EOVERFLOW);

    struct doomdev* dev = surface->parent_device;

    struct doomdev_column data;
    if (copy_from_user(&data, user_data, sizeof(struct doomdev_column)))
        return -EINVAL; // @todo jaki blad?

    if (data.y1 > data.y2) {
        printk(KERN_NOTICE "DRAW_COLUMN: y1 > y2!");
        return -EINVAL;
    }

    if (data.x > surface->width || data.y2 > surface->height) {
        printk(KERN_NOTICE "DRAW_COLUMN: x > width or y2 > height");
        return -EINVAL;
    }

    /*
     * This check wasn't consistent in polish assignment
     * > SPAN: (wysokie 10 bitów powinno być zignorowane przez sterownik)
     * > COLUMN: liczba stałoprzecinkowa 16.16 bez znaku, musi być w zakresie obsługiwanym przez sprzęt
     * but English version says it should be in the range supported by the hardware in both cases
     */
    if ((data.ustart &~HARDDOOM_U_COORD_MASK) || (data.ustep &~HARDDOOM_U_COORD_MASK)) {
        printk(KERN_NOTICE "DRAW_COLUMN: ustart or ustep not in format 10.16");
        return -EINVAL;
    }

    if ((info->draw_flags & (DOOMDEV_DRAW_FLAGS_FUZZ | DOOMDEV_DRAW_FLAGS_COLORMAP)) && (data.colormap_idx != dev->last_colormapidx)) {
        if (doom_set_colormap_new(batch, info->colormaps_fd, data.colormap_idx, 0)) {
            printk(KERN_NOTICE "DRAW_COLUMN: colormap index out of range");
            return -EINVAL;
        } else {
            dev->last_colormapidx = data.colormap_idx;
        }
    }

    if ((info->draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ) == 0) {
        push_doom_batch(batch, HARDDOOM_CMD_USTART(data.ustart));
        push_doom_batch(batch, HARDDOOM_CMD_USTEP(data.ustep));
    }

    push_doom_batch(batch, HARDDOOM_CMD_XY_A(data.x, data.y1));
    push_doom_batch(batch, HARDDOOM_CMD_XY_B(data.x, data.y2));

    push_doom_batch(batch, HARDDOOM_CMD_DRAW_COLUMN(data.texture_offset));

    return 0;
}

static int doom_draw_span(struct surface* surface, struct doom_batch* batch,  struct doomdev_surf_ioctl_draw_spans* info, const struct doomdev_span __user* user_data)
{
    ENSURE_FREE_SLOTS(batch, SIZE_DRAW_SPAN, -EOVERFLOW);

    struct doomdev* dev = surface->parent_device;
    struct doomdev_span data;

    if (copy_from_user(&data, user_data, sizeof(struct doomdev_span)))
        return -EINVAL; // @todo jaki blad?

    if (data.x1 > data.x2) {
        printk(KERN_NOTICE "DRAW_SPAN: x1 > x2");
        return -EINVAL;
    }

    if (data.x2 > surface->width || data.y > surface->height) {
        printk(KERN_NOTICE "DRAW_SPAN x2 > width or y > height");
        return -EINVAL;
    }

    /*
     * This check wasn't consistent in polish assignment
     * > SPAN: (wysokie 10 bitów powinno być zignorowane przez sterownik)
     * > COLUMN: liczba stałoprzecinkowa 16.16 bez znaku, musi być w zakresie obsługiwanym przez sprzęt
     * but English version says it should be in the range supported by the hardware in both cases
     */
    if ((data.ustart &~HARDDOOM_U_COORD_MASK) ||
            (data.ustep &~HARDDOOM_U_COORD_MASK) ||
            (data.vstep &~HARDDOOM_U_COORD_MASK) ||
            (data.vstart &~HARDDOOM_U_COORD_MASK)) {
        printk(KERN_NOTICE "DRAW_SPAN: ustart/vstart/ustep/vstep out of range");
        return -EINVAL;
    }

    if ((info->draw_flags & DOOMDEV_DRAW_FLAGS_COLORMAP) && (data.colormap_idx != dev->last_colormapidx)) {
        if (doom_set_colormap_new(batch, info->colormaps_fd, data.colormap_idx, 0)) {
            printk(KERN_NOTICE "DRAW_SPAN: color map invalid index");
            return -EINVAL;
        } else {
            dev->last_colormapidx = data.colormap_idx;
        }
    }

    push_doom_batch(batch, HARDDOOM_CMD_USTART(data.ustart));
    push_doom_batch(batch, HARDDOOM_CMD_VSTART(data.vstart));
    push_doom_batch(batch, HARDDOOM_CMD_USTEP(data.ustep));
    push_doom_batch(batch, HARDDOOM_CMD_VSTEP(data.vstep));

    push_doom_batch(batch, HARDDOOM_CMD_XY_A(data.x1, data.y));
    push_doom_batch(batch, HARDDOOM_CMD_XY_B(data.x2, data.y));

    push_doom_batch(batch, HARDDOOM_CMD_DRAW_SPAN);

    return 0;
}


int doom_draw_background(struct surface* surface, struct flat_texture* texture)
{
    struct doom_batch batch;

    if (init_doom_batch(&batch, SIZE_DRAW_BACKGROUND))
        return -ENOMEM;

    doom_set_dst_surf_new(surface, &batch);
    doom_set_flat_texture(&batch, texture);
    push_doom_batch(&batch, HARDDOOM_CMD_DRAW_BACKGROUND);

    int result = send_commands(surface->parent_device, &batch);

    free_doom_batch(&batch);

    if (!result)
        surface->is_diry = 1;

    return result;
}

int doom_draw_lines(struct surface* surface, const struct doomdev_line __user* data, int lines_num)
{
    struct doom_batch batch;

    if (init_doom_batch(&batch, SIZE_SET_SURFACE + SIZE_DRAW_LINE * lines_num))
        return -ENOMEM;

    if (doom_set_dst_surf_new(surface, &batch)) {
        free_doom_batch(&batch);
        return -ENOMEM;
    }

    int processed = 0;
    int result_code = 0;
    for (int i = 0; i < lines_num; ++i) {
        result_code = doom_draw_line(surface, &batch, data + i);
        if (result_code)
            break; // no more space or error
        processed++;

    }
    int result = send_commands(surface->parent_device, &batch);

    free_doom_batch(&batch);

    if (result)
        return result;
    else
        surface->is_diry = 1;

    return processed > 0 ? processed : result_code;
}


int doom_copy_rects(struct surface* dst, struct surface* src, const struct doomdev_copy_rect __user* data, int rects_num)
{
    struct doom_batch batch;

    if (dst->width != src->width || dst->height != src->height) {
        printk(KERN_NOTICE "COPY_RECT: src and dst have different size");
        return -EINVAL;
    }

    if (init_doom_batch(&batch, SIZE_COPY_RECT_HEADER  + SIZE_COPY_RECT * rects_num))
        return -ENOMEM;

    doom_set_dst_surf_new(dst, &batch);
    doom_set_src_surf_new(src, &batch);

    if (src->is_diry)
        push_doom_batch(&batch, HARDDOOM_CMD_INTERLOCK);

    int processed = 0;
    int result_code = 0;

    for (int i = 0; i < rects_num; ++i) {
        result_code = doom_copy_rect(dst, src, &batch, data + i);
        if (result_code)
            break;
        processed++;
    }

    int result = send_commands(dst->parent_device, &batch);

    free_doom_batch(&batch);

    if (result)
        return result;
    else {
        dst->is_diry = 1;
        src->is_diry = 0;
    }

    return processed > 0 ? processed : result_code;
}

int doom_fill_rects(struct surface* surface, const struct doomdev_fill_rect __user* data, int rects_num)
{
    struct doom_batch batch;

    if (init_doom_batch(&batch, SIZE_FILL_RECT_HEADER + SIZE_FILL_RECT * rects_num))
        return -ENOMEM;

    doom_set_dst_surf_new(surface, &batch);

    int processed = 0;
    int result_code = 0;

    for (int i = 0; i < rects_num; ++i) {
        result_code = doom_fill_rect(surface, &batch, data + i);
        if (result_code)
            break;
        processed++;
    }

    int result = send_commands(surface->parent_device, &batch);

    free_doom_batch(&batch);

    if (result)
        return result;
    else
        surface->is_diry = 1;

    return processed > 0 ? processed : result_code;
}


int doom_draw_spans(struct surface* surface, struct doomdev_surf_ioctl_draw_spans* info, struct flat_texture* texture, const struct doomdev_span __user* data, int spans_num)
{
    struct doom_batch batch;

    if (init_doom_batch(&batch, SIZE_DRAW_SPAN_HEADER  + SIZE_DRAW_SPAN * spans_num))
        return -ENOMEM;

    doom_set_dst_surf_new(surface, &batch);
    doom_set_flat_texture(&batch, texture);

    push_doom_batch(&batch, HARDDOOM_CMD_DRAW_PARAMS(info->draw_flags));
    if (info->draw_flags & DOOMDEV_DRAW_FLAGS_TRANSLATE) {
        doom_set_colormap_new(&batch, info->translations_fd, info->translation_idx, 1);
    }

    int processed = 0;
    int result_code = 0;
    surface->parent_device->last_colormapidx = -1;

    for (int i = 0; i < spans_num; ++i) {
        result_code = doom_draw_span(surface, &batch, info, data + i);
        if (result_code)
            break;
        processed++;
    }

    int result = send_commands(surface->parent_device, &batch);

    free_doom_batch(&batch);

    if (result)
        return result;
    else
        surface->is_diry = 1;

    return processed > 0 ? processed : result_code;
}


int doom_draw_columns(struct surface* surface, struct doomdev_surf_ioctl_draw_columns* info, const struct doomdev_column __user* data, int columns_num)
{
    struct doom_batch batch;

    if (init_doom_batch(&batch, SIZE_SET_SURFACE + SIZE_DRAW_COLUMN_HEADER  + SIZE_DRAW_COLUMN * columns_num))
        return -ENOMEM;

    if (doom_set_dst_surf_new(surface, &batch)) {
        free_doom_batch(&batch);
        return -ENOMEM;
    }

    if (info->draw_flags & DOOMDEV_DRAW_FLAGS_FUZZ) {
        push_doom_batch(&batch, HARDDOOM_CMD_DRAW_PARAMS(DOOMDEV_DRAW_FLAGS_FUZZ));
    }
    else {
        push_doom_batch(&batch, HARDDOOM_CMD_DRAW_PARAMS(info->draw_flags));
        doom_set_texture_new(info->texture_fd, &batch);

        if (info->draw_flags & DOOMDEV_DRAW_FLAGS_TRANSLATE) {
            doom_set_colormap_new(&batch, info->translations_fd, info->translation_idx, 1);
        }
    }

    int processed = 0;
    int result_code = 0;

    surface->parent_device->last_colormapidx = -1;

    for (int i = 0; i < columns_num; ++i) {
        result_code = doom_draw_column(&batch, surface, info, data + i);
        if (result_code)
            break;
        processed++;
    }

    int result = send_commands(surface->parent_device, &batch);

    free_doom_batch(&batch);

    if (result)
        return result;
    else
        surface->is_diry = 1;

    return processed > 0 ? processed : result_code;
}
