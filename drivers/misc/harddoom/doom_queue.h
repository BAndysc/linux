#ifndef DOOM_QUEUE_H
#define DOOM_QUEUE_H

#include <linux/types.h>

struct doomdev;

struct doom_batch
{
    u32* array;
    size_t size;
    size_t index;
};

/**
 * @brief Initializes doom batch structure to batch commands
 * @param batch - struct to initialize
 * @param size - expected number of arguments. Real size can be smaller
 * @return 0 if success, -ENOMEM if couldn't allocate memory
 */
int init_doom_batch(struct doom_batch* batch, u32 size);

/**
 * @brief Tries to push command to batch
 * @return 0 if command was pushed, -EOVERFLOW if command cannot be pushed (batch full)
 */
int push_doom_batch(struct doom_batch* batch, u32 command);

/**
 * @brief Frees memory allocated in `init_doom_batch`
 * @param batch to free
 */
void free_doom_batch(struct doom_batch* batch);

/**
 * @return number of free slots in batch
 */
size_t doom_batch_available(struct doom_batch* batch);

#define ENSURE_FREE_SLOTS(batch, req, err) if (doom_batch_available(batch) < (req)) return (err);

/**
 * @brief Automically sends commands to device. Blocks if there is no place in device queue
 * @return 0 if success, negative value if lock failed
 */
int send_commands(struct doomdev* dev, struct doom_batch* batch);

/**
 * @brief Ensures queue is empty, blocks if needed. After task is done, you must call unlock_device
 */
int lock_device(struct doomdev* dev);

/**
 * @brief Unlocks device locked by lock_device
 */
void unlock_device(struct doomdev* dev);

#define COMMANDS_BETWEEN_ASYNC 32
#define DOOM_QUEUE_SIZE 32
#define DOOM_BATCH_SIZE 2

#endif // DOOM_QUEUE_H
