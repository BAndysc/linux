#ifndef DOOM_INTERNAL_H
#define DOOM_INTERNAL_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>

#include <linux/types.h>
#include <linux/cdev.h>

// struct file
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>

// kmalloc
#include <linux/slab.h>

// userspace-kernel transfer
#include <asm/uaccess.h>
#include <asm/iomap.h>

// interrupts
#include <linux/interrupt.h>

// sync
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>

#include "doom_pagetable.h"
#include "doom_queue.h"

#define DRV_NAME "HARDDOOM_BK"
#define BAR0 0
#define BAR0_SIZE 4096
#define DOOM_PAGE_SIZE 4096
#define DOOM_PAGE_ALIGN 4096
#define PTR_ADV(x, y) (((char __iomem*)(x))+y)


#define PASS_ERROR_IF_ANY(VAL)      \
    do {                            \
        int error_code = VAL;       \
        if (error_code)             \
            return error_code;      \
    } while (0)


struct doomdev {
    struct pci_dev *pcidev;
    dev_t cdev;
    struct cdev cdev_struct;
    struct device *char_device;
    void __iomem *bar;

    int global_index;

    // big mutex for everything. Not best solution, but works
    struct mutex big_doom_lock;

    // completion to wait for ping async
    struct completion pong_async;

    // completion to wait for ping sync
    struct completion ping_sync;

    // previous cached draw params
    dma_addr_t last_dst;
    u32 last_width;
    u32 last_height;
    s32 last_colormapidx;

    // number of commands in queue since last ping_async
    u32 commands_since_last_async;
};


#endif
