#ifndef DOOM_PAGETABLE_H
#define DOOM_PAGETABLE_H

struct doompage
{
    void* mem_addr;
    void* memory;

    dma_addr_t dma_addr;
    dma_addr_t dma;

    size_t size;
};

struct doom_page_table
{
    struct doompage table;
    struct doompage* pages;

    int size;
};

struct doomdev;

#define MAX_PAGES 1024

/**
 * @brief Allocates new DMA page for DOOM device in given size
 * @param pointer to struct representing doom device associated with page
 * @param pointer to write pointers to new page
 * @param size in bytes of page
 * @param alignment of page
 * @return 0 on success, -ENOMEM when alloc memory fails
 */
int alloc_doom_page(struct doomdev* dev, struct doompage* p, size_t page_size, size_t align);

/**
 * @brief Frees DMA page allocated by `alloc_doom_page`
 * @param pointer to struct representing doom device associated with page
 * @param acual page
 */
void free_doom_page(struct doomdev* dev, struct doompage* page);

/**
 * @brief Frees HARDDOOM page table allocated by `create_pagetable`
 * @param dev
 * @param table
 */
void free_pagetable(struct doomdev* dev, struct doom_page_table* table);

/**
 * @brief Allocates new HARDDOOM page table for device in given size
 * @param pointer to struct representing doom device associated with page
 * @param pointer to struct representing page table
 * @param size in bytes of page
 * @param page alignment
 * @param number of pages
 * @return 0 if success, -ENOMEM when no memory, -EOVERFLOW when request more than 1024 pages
 */
int create_pagetable(struct doomdev* dev, struct doom_page_table* table, size_t page_size, size_t align, size_t pages_num);

#endif
