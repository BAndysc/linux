#ifndef DOOM_RENDER_H
#define DOOM_RENDER_H

#include "doom_internal.h"
#include "harddoom.h"
#include "doomdev.h"

struct surface;
struct texture;
struct flat_texture;
struct colormap;

/**
 * @brief Sends DRAW_BACKGROUND command do device. Blocks when there is no place in queue.
 * @return 0 on success, -ENOMEM when no memory, other negative value if send is interrupted (look at `send_commands`)
 */
int doom_draw_background(struct surface* surface, struct flat_texture* texture);

/**
 * @brief Sends multiple DRAW_LINES commands do device. Blocks when there is no place in queue.
 * @return on success number of successful operations, when all failed: -ENOMEM when no memory,
 * other negative value if send is interrupted (look at `send_commands`)
 */
int doom_draw_lines(struct surface* surface, const struct doomdev_line __user* data, int lines_num);

/**
 * @brief Sends multiple FILL_RECT commands do device. Blocks when there is no place in queue.
 * @return on success number of successful operations, when all failed: -ENOMEM when no memory,
 * other negative value if send is interrupted (look at `send_commands`)
 */
int doom_fill_rects(struct surface* surface, const struct doomdev_fill_rect __user* data, int rects_num);

/**
 * @brief Sends multiple COPY_RECTS commands do device. Blocks when there is no place in queue.
 * @return on success number of successful operations, when all failed: -ENOMEM when no memory,
 * other negative value if send is interrupted (look at `send_commands`)
 */
int doom_copy_rects(struct surface* dst, struct surface* src, const struct doomdev_copy_rect __user* data, int rects_num);

/**
 * @brief Sends multiple DRAW_COLUMNS commands do device. Blocks when there is no place in queue.
 * @return on success number of successful operations, when all failed: -ENOMEM when no memory,
 * other negative value if send is interrupted (look at `send_commands`)
 */
int doom_draw_columns(struct surface* surface, struct doomdev_surf_ioctl_draw_columns* info, const struct doomdev_column __user* data, int columns_num);

/**
 * @brief Sends multiple DRAW_SPANS commands do device. Blocks when there is no place in queue.
 * @return on success number of successful operations, when all failed: -ENOMEM when no memory,
 * other negative value if send is interrupted (look at `send_commands`)
 */
int doom_draw_spans(struct surface* surface, struct doomdev_surf_ioctl_draw_spans* info, struct flat_texture* texture, const struct doomdev_span __user* data, int spans_num);

// (10.16 format)
#define HARDDOOM_U_COORD_MASK 0x3ffffff

#endif // DOOM_RENDER_H
