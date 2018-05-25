#include "harddoom.h"
#include "doomdev.h"
#include "doomcode.h"

#include "doom_pagetable.h"
#include "doom_internal.h"
#include "doom_render.h"
#include "doom_texture.h"
#include "doom_surface.h"
#include "doom_ioctl.h"

#define MAX_DOOM_DEVICES 256

static struct pci_device_id ids[] = {
    { PCI_DEVICE(HARDDOOM_VENDOR_ID, HARDDOOM_DEVICE_ID), },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

struct class doom_class = {
    .name = "harddoom",
    .owner = THIS_MODULE,
};

static struct mutex global_harddoom_mutex;

static struct doomdev* global_doom_device_array[MAX_DOOM_DEVICES];

static dev_t first_cdev;

static int harddoom_open(struct inode *inode, struct file *filp)
{
    struct doomdev *dev; /* device information */

    dev = container_of(inode->i_cdev, struct doomdev, cdev_struct);
    filp->private_data = dev;

    return 0;
}

static int hardoom_release(struct inode *inode, struct file *filp)
{
    struct doomdev *dev = filp->private_data;

    // let's wait for everything to finish
    PASS_ERROR_IF_ANY(lock_device(dev));

    unlock_device(dev);

    return 0;
}

static long harddoom_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct doomdev* dev = filp->private_data;

    switch (cmd) {
        case DOOMDEV_IOCTL_CREATE_SURFACE:
        {
            struct doomdev_ioctl_create_surface surf_arg;

            if (copy_from_user(&surf_arg, (const void __user *) arg, sizeof(struct doomdev_ioctl_create_surface)))
                goto err;

            return ioctl_create_surface(dev, &surf_arg);
        }

        case DOOMDEV_IOCTL_CREATE_FLAT:
        {
            struct doomdev_ioctl_create_flat tex_arg;

            if (copy_from_user(&tex_arg, (const void __user *) arg, sizeof(struct doomdev_ioctl_create_flat)))
                goto err;

            return ioctl_create_flat(dev, &tex_arg);
        }

        case DOOMDEV_IOCTL_CREATE_TEXTURE:
        {
            struct doomdev_ioctl_create_texture tex_arg;

            if (copy_from_user(&tex_arg, (const void __user *) arg, sizeof(struct doomdev_ioctl_create_texture)))
                goto err;

            return ioctl_create_texture(dev, &tex_arg);
        }
 
        case DOOMDEV_IOCTL_CREATE_COLORMAPS:
        {
            struct doomdev_ioctl_create_colormaps args;

            if (copy_from_user(&args, (const void __user *) arg, sizeof(struct doomdev_ioctl_create_colormaps)))
                goto err;

            return ioctl_create_colormap(dev, &args);
        }
    }

err:
    return -EINVAL;
}


static struct file_operations pci_fops = {
    .owner              =  THIS_MODULE,
    .unlocked_ioctl     =  &harddoom_ioctl,
    .compat_ioctl       =  &harddoom_ioctl,
    .open               =  &harddoom_open,
    .release            =  &hardoom_release,
};
 
static irqreturn_t harddoom_irq(int irq, void* data)
{
    struct doomdev* dev = data;

    u32 intr = ioread32(PTR_ADV(dev->bar, HARDDOOM_INTR));
    u32 intr_enb = ioread32(PTR_ADV(dev->bar, HARDDOOM_INTR_ENABLE));

    if ((intr & intr_enb) == 0)
        return IRQ_NONE;

    iowrite32(intr & intr_enb, PTR_ADV(dev->bar, HARDDOOM_INTR));

    if ((intr & HARDDOOM_INTR_PONG_SYNC) && (intr_enb & HARDDOOM_INTR_PONG_SYNC))
        complete(&dev->ping_sync);

    if ((intr & HARDDOOM_INTR_PONG_ASYNC) && (intr_enb & HARDDOOM_INTR_PONG_ASYNC))
        complete(&dev->pong_async);

    if ((intr & HARDDOOM_INTR_FE_ERROR) && (intr_enb & HARDDOOM_INTR_FE_ERROR)) {
        u32 err = ioread32(PTR_ADV(dev->bar, HARDDOOM_FE_ERROR_CODE));
        u32 data = ioread32(PTR_ADV(dev->bar, 0x2c));

        printk(KERN_WARNING "Hardoom: FE ERROR: %x DATA: %x\n", err, data);
    }

    return IRQ_HANDLED;
}
 
 
static int device_startup(struct doomdev* dev)
{
    iowrite32(0, PTR_ADV(dev->bar, HARDDOOM_FE_CODE_ADDR));

    for (int i = 0; i < ARRAY_SIZE(doomcode); ++i)
        iowrite32(doomcode[i], PTR_ADV(dev->bar, HARDDOOM_FE_CODE_WINDOW));

    iowrite32(HARDDOOM_RESET_ALL, PTR_ADV(dev->bar, HARDDOOM_RESET));

    iowrite32(HARDDOOM_INTR_MASK, PTR_ADV(dev->bar, HARDDOOM_INTR));

    iowrite32(HARDDOOM_INTR_MASK & ~ HARDDOOM_INTR_PONG_ASYNC, PTR_ADV(dev->bar, HARDDOOM_INTR_ENABLE));

    iowrite32(HARDDOOM_ENABLE_ALL & ~ HARDDOOM_ENABLE_FETCH_CMD, PTR_ADV(dev->bar, HARDDOOM_ENABLE));

    return 0;
}

static int harddoom_find_first_free_index(void)
{
    for (int i = 0; i < ARRAY_SIZE(global_doom_device_array); ++i) {
        if (!global_doom_device_array[i])
            return i;
    }
    return -EOVERFLOW;
}

static int hardoom_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    printk("Hardoom installed");
    if (pci_enable_device(dev)) {
        dev_err(&dev->dev, "can't enable PCI device\n");
        return -ENODEV;
    }

    struct doomdev* doomdata = kmalloc(sizeof(struct doomdev), GFP_KERNEL);
    if (IS_ERR_OR_NULL(doomdata)) {
        dev_err(&dev->dev, "No memory!");
        goto kmalloc_fail;
    }
    doomdata->commands_since_last_async = 0;
    mutex_init(&doomdata->big_doom_lock);
    init_completion(&doomdata->ping_sync);
    init_completion(&doomdata->pong_async);
    doomdata->pcidev = dev;

    pci_set_drvdata(dev, doomdata);


    if (pci_request_regions(dev, DRV_NAME) != 0) {
        dev_err(&dev->dev, "Device busy");
        goto pci_request_regions_fail;
    }
    
    doomdata->bar = pci_iomap(dev, BAR0, BAR0_SIZE);
    if (IS_ERR_OR_NULL(doomdata->bar)) {
        dev_err(&dev->dev, "IOMap error");
        goto pci_iomap_fail;
    }

    pci_set_master(dev);

    pci_set_dma_mask(dev, DMA_BIT_MASK(32));
    pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(32));

    if (request_irq(dev->irq, &harddoom_irq, IRQF_SHARED, "doom", doomdata) != 0) {
        dev_err(&dev->dev, "Cannot register interrupt");
        goto request_irq_fail;
    }

    mutex_lock(&global_harddoom_mutex);

    doomdata->global_index = harddoom_find_first_free_index();

    if (doomdata->global_index < 0) {
        dev_err(&dev->dev, "Too many devices, no more minors");
        goto nominor;
    }

    doomdata->cdev = MKDEV(MAJOR(first_cdev), MINOR(first_cdev) + doomdata->global_index);

    cdev_init(&doomdata->cdev_struct, &pci_fops);
    int err = cdev_add (&doomdata->cdev_struct, doomdata->cdev, 1);
    if (err) {
        dev_err(&dev->dev, "cdev_add failed: %d", err);
        goto cdev_add_fail;
    }

    doomdata->char_device = device_create(&doom_class, &dev->dev, doomdata->cdev, NULL, "doom%d", doomdata->global_index);

    if (!doomdata->char_device) {
        printk (KERN_NOTICE "Error creating device");
        goto device_create_fail;
    }

    printk("Create device ok /dev/doom%d\n", doomdata->global_index);

    if (device_startup(doomdata))
        goto startup_error;

    global_doom_device_array[doomdata->global_index] = doomdata;
    mutex_unlock(&global_harddoom_mutex);

    return 0;

    startup_error:

    device_destroy(&doom_class, doomdata->cdev);

    device_create_fail:
    cdev_del(&doomdata->cdev_struct);

    cdev_add_fail:

    nominor:
    mutex_unlock(&global_harddoom_mutex);

    free_irq(dev->irq, doomdata);

    request_irq_fail:
    pci_clear_master(dev);

    pci_iounmap(dev, doomdata->bar);

    pci_iomap_fail:
    pci_release_regions(dev);

    pci_request_regions_fail:
    kfree(doomdata);

    kmalloc_fail:
    pci_disable_device(dev);


    return -1;
}

static void remove(struct pci_dev *dev)
{
    printk("Harddoom remove\n");

    struct doomdev* doomdata = pci_get_drvdata(dev);
    lock_device(doomdata); // if failed, what can we do? nothing

    iowrite32(0, PTR_ADV(doomdata->bar, HARDDOOM_ENABLE));
    iowrite32(0, PTR_ADV(doomdata->bar, HARDDOOM_ENABLE));
    iowrite32(HARDDOOM_RESET_ALL, PTR_ADV(doomdata->bar, HARDDOOM_RESET));
    ioread32(PTR_ADV(doomdata->bar, HARDDOOM_FIFO_FREE));

    free_irq(dev->irq, doomdata);
    pci_clear_master(dev);

    device_destroy(&doom_class, doomdata->cdev);
    cdev_del(&doomdata->cdev_struct);

    pci_iounmap(dev, doomdata->bar);
    pci_release_regions(dev);
    pci_disable_device(dev);

    unlock_device(doomdata);
    mutex_lock(&global_harddoom_mutex);
    global_doom_device_array[doomdata->global_index] = NULL;
    mutex_unlock(&global_harddoom_mutex);
    kfree(doomdata);
}

static struct pci_driver pci_driver = {
    .name = "harddoom",
    .id_table = ids,
    .probe = hardoom_probe,
    .remove = remove,
};

static int __init harddoom_init(void)
{
    mutex_init(&global_harddoom_mutex);

    memset(global_doom_device_array, 0, sizeof(void*) * ARRAY_SIZE(global_doom_device_array));
    int result = class_register(&doom_class);

    if (result)
        return result;

    result = alloc_chrdev_region(&first_cdev, 0, MAX_DOOM_DEVICES, "doom");

    if (result)
        return result;

    return pci_register_driver(&pci_driver);
}

static void __exit harddoom_exit(void)
{
    pci_unregister_driver(&pci_driver);
    unregister_chrdev_region(first_cdev, MAX_DOOM_DEVICES);
    class_unregister(&doom_class);
}

MODULE_AUTHOR("Bartosz Korczynski");
MODULE_DESCRIPTION("Synchronous DOOM driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

module_init(harddoom_init);
module_exit(harddoom_exit);
