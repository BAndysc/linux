#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include "doom_pagetable.h"
#include "doom_internal.h"

#define MAKE_PAGE_SIZE(S, A) ((S)+(A)-1)
#define ENCODE_PAGE(addr) (1 | ((addr)))

int alloc_doom_page(struct doomdev* dev, struct doompage* p, size_t page_size, size_t align) {
    p->size = MAKE_PAGE_SIZE(page_size, align);
    p->mem_addr = dma_alloc_coherent(&dev->pcidev->dev, p->size, &p->dma_addr, GFP_KERNEL);

    if (IS_ERR_OR_NULL(p->mem_addr))
        return -ENOMEM;

    p->memory = PTR_ALIGN(p->mem_addr, align);
    p->dma = ALIGN(p->dma_addr, align);

    return 0;
}

void free_doom_page(struct doomdev* dev, struct doompage* page)
{
    dma_free_coherent(&dev->pcidev->dev, page->size, page->mem_addr, page->dma_addr);
}

void free_pagetable(struct doomdev* dev, struct doom_page_table* table)
{
    for (int i = 0; i < table->size; ++i)
        free_doom_page(dev, table->pages + i);

    free_doom_page(dev, &table->table);

    kfree(table->pages);
}

int create_pagetable(struct doomdev* dev, struct doom_page_table* table, size_t page_size, size_t align, size_t pages_num)
{
    if (pages_num > MAX_PAGES)
        return -EOVERFLOW;

    if (alloc_doom_page(dev, &table->table, page_size, align))
        return -ENOMEM;

    struct doompage* ptr = kmalloc(sizeof(struct doompage) * pages_num, GFP_KERNEL);

    if (IS_ERR_OR_NULL(ptr))
        goto error_alloc_array;

    table->pages = ptr;
    table->size = 0;

    for (int i = 0; i < pages_num; ++i) {

        if (alloc_doom_page(dev, table->pages + i, page_size, align))
            goto err;

        u32 entry = ENCODE_PAGE(table->pages[i].dma);
        ((u32*)table->table.memory)[i] = entry;
        table->size++;
    }

    return 0;

    err:
    // error occured, but first we need to free already allocated pages
    free_pagetable(dev, table);

    error_alloc_array:
    free_doom_page(dev, &table->table);

    return -ENOMEM;
}
