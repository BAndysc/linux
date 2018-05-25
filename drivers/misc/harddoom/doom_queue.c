#include "doom_internal.h"
#include "doom_queue.h"
#include "harddoom.h"

#define MIN(x, y) ((x)<(y)?(x):(y))

int init_doom_batch(struct doom_batch* batch, u32 size)
{
    if (size * sizeof(u32) > KMALLOC_MAX_SIZE)
        size = KMALLOC_MAX_SIZE / sizeof(u32);

    u32* array = kmalloc(size * sizeof(u32), GFP_KERNEL);

    if (IS_ERR_OR_NULL(array))
        return -ENOMEM;

    batch->array = array;
    batch->size = size;
    batch->index = 0;

    return 0;
}

int push_doom_batch(struct doom_batch* batch, u32 command)
{
    if (batch->index == batch->size)
        return -EOVERFLOW;

    batch->array[batch->index++] = command;

    return 0;
}

void free_doom_batch(struct doom_batch* batch)
{
    kfree(batch->array);
}

size_t doom_batch_available(struct doom_batch* batch)
{
    return batch->size - batch->index;
}

static void send_command_one(struct doomdev* dev, u32 cmd)
{
    u32 free;
    do {
        spinlock_t mLock;
        spin_lock_init(&mLock);
        unsigned long flags;

        spin_lock_irqsave(&mLock, flags);

        free = ioread32(PTR_ADV(dev->bar, HARDDOOM_FIFO_FREE));

        // one slot for command, one slot for async
        if (free < 2) {
            iowrite32(HARDDOOM_INTR_PONG_ASYNC, PTR_ADV(dev->bar, HARDDOOM_INTR));
            iowrite32(HARDDOOM_INTR_MASK | HARDDOOM_INTR_PONG_ASYNC, PTR_ADV(dev->bar, HARDDOOM_INTR_ENABLE));

            free = ioread32(PTR_ADV(dev->bar, HARDDOOM_FIFO_FREE));
            if (free >= 2) {
                spin_unlock_irqrestore(&mLock, flags);
                break;
            }
            spin_unlock_irqrestore(&mLock, flags);

            wait_for_completion(&dev->pong_async);

            iowrite32(HARDDOOM_INTR_MASK &~ HARDDOOM_INTR_PONG_ASYNC, PTR_ADV(dev->bar, HARDDOOM_INTR_ENABLE));
        } else
            spin_unlock_irqrestore(&mLock, flags);

    } while (free < 2);


    iowrite32(cmd, PTR_ADV(dev->bar, HARDDOOM_FIFO_SEND));

    dev->commands_since_last_async++;

    if ((dev->commands_since_last_async % COMMANDS_BETWEEN_ASYNC) == 0) {
        iowrite32(HARDDOOM_CMD_PING_ASYNC, PTR_ADV(dev->bar, HARDDOOM_FIFO_SEND));
        dev->commands_since_last_async = 0;
    }
}

static void send_commands_batch(struct doomdev* dev, struct doom_batch* batch)
{
    int i = 0;

    if (HARDDOOM_CMD_EXTR_TYPE(batch->array[0]) == HARDDOOM_CMD_TYPE_SURF_DIMS) {
        if (HARDDOOM_CMD_EXTR_SURF_DIMS_WIDTH(batch->array[0]) == dev->last_width && HARDDOOM_CMD_EXTR_SURF_DIMS_HEIGHT(batch->array[0]) == dev->last_height) {

            if (HARDDOOM_CMD_EXTR_TYPE(batch->array[1]) == HARDDOOM_CMD_TYPE_SURF_DST_PT) {
                if (HARDDOOM_CMD_EXTR_PT(batch->array[1]) == dev->last_dst)
                    i++;
                else
                    dev->last_dst = HARDDOOM_CMD_EXTR_PT(batch->array[1]);
            }
            i++;
        } else {
            dev->last_width = HARDDOOM_CMD_EXTR_SURF_DIMS_WIDTH(batch->array[0]);
            dev->last_height = HARDDOOM_CMD_EXTR_SURF_DIMS_HEIGHT(batch->array[0]);
            if (HARDDOOM_CMD_EXTR_TYPE(batch->array[1]) == HARDDOOM_CMD_TYPE_SURF_DST_PT)
                dev->last_dst = HARDDOOM_CMD_EXTR_PT(batch->array[1]);
        }
    }

    for (; i < batch->index; ++i) {
        send_command_one(dev, batch->array[i]);
    }
}

int send_commands(struct doomdev* dev, struct doom_batch* batch)
{
    PASS_ERROR_IF_ANY(mutex_lock_interruptible(&dev->big_doom_lock));

    send_commands_batch(dev, batch);

    unlock_device(dev);

    return 0;
}

int lock_device(struct doomdev* dev)
{
    int lock_result = mutex_lock_interruptible(&dev->big_doom_lock);
    if (lock_result) // e.g. -EINTR
        return lock_result;

    send_command_one(dev, HARDDOOM_CMD_PING_SYNC);

    lock_result = wait_for_completion_interruptible(&dev->ping_sync);
    if (lock_result)
        return lock_result;

    return 0;
}

void unlock_device(struct doomdev* dev)
{
    mutex_unlock(&dev->big_doom_lock);
}
