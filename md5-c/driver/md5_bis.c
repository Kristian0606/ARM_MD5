//
// Created by jbardaghji on 28/05/23.
//

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/tty.h>
#include <linux/sched/signal.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#define  DEVICE_NAME "MD5_module"
#define  CLASS_NAME  "MD5"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack Bardaghji");
MODULE_DESCRIPTION("A Linux char driver for a MD5 crypto core");
MODULE_VERSION("0.1");

// *****************************************************************************
// *                            MD5 IMPL                                       *
// *****************************************************************************

// Per-round shift amounts
static const uint32_t r[] = {7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
                             5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
                             4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
                             6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21};

// Use binary integer part of the sines of integers (in radians) as constants
static const uint32_t k[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                             0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                             0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                             0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                             0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                             0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                             0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                             0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                             0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                             0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                             0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                             0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                             0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                             0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                             0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                             0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

// left rotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))
#define MD5_HASH_SIZE 16

ssize_t md5sum(uint32_t *h, const char *ubuff, size_t initial_len) {
    uint8_t *msg = NULL;
    uint32_t temp, bits_len;
    int new_len, offset;

    h[0] = 0x67452301;
    h[1] = 0xefcdab89;
    h[2] = 0x98badcfe;
    h[3] = 0x10325476;

    new_len = ((((initial_len + 8) / 64) + 1) * 64) - 8;

/* allocate memory */
    msg = kmalloc(new_len + 64, GFP_KERNEL);
    if (!msg)
        return -ENOMEM;
    memset(msg, 0, new_len + 64);

    if (copy_from_user(msg, ubuff, initial_len))
        return -EFAULT;

    msg[initial_len] = 128;

    bits_len = 8*initial_len;
    memcpy(msg + new_len, &bits_len, 4);


    for(offset=0; offset<new_len; offset += (512/8)) {

        uint32_t *w = (uint32_t *) (msg + offset);
        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t i;

        for(i = 0; i<64; i++) {
            uint32_t f, g;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5*i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3*i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7*i) % 16;
            }
            temp = d;
            d = c;
            c = b;
            b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
            a = temp;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
    }
// cleanup
    kfree(msg);
    return 0;
}
// *****************************************************************************
// *             TTY structure(linked list) to communicate with terminal    *
// *****************************************************************************
struct tty_listitem {
    dev_t key;
    uint8_t hash[MD5_HASH_SIZE];
    size_t index;
    struct list_head list;
};


// the list of devices and a lock to protect it
static LIST_HEAD(tty_list);
static DEFINE_SPINLOCK(tty_list_lock);

static struct tty_listitem *md5_lookfor_tty(dev_t key) {
    struct tty_listitem *lptr;

    list_for_each_entry(lptr, &tty_list, list) {
        if (lptr->key == key)
            return lptr;
    }

    lptr = kmalloc(sizeof(struct tty_listitem), GFP_KERNEL);
    if (!lptr) /* no memory */
        return NULL;

    /* init tty */
    memset(lptr, 0, sizeof(struct tty_listitem));
    lptr->key = key;
    lptr->index = 0;

    list_add(&lptr->list, &tty_list);

    return lptr;
}

static int    majorNumber;
static struct class*  MD5charClass  = NULL;
static struct cdev md5_cdev;

// The prototype functions for the character driver -- must come before the struct definition
static int     MD5_open_close(struct inode *, struct file *);
static ssize_t MD5_read(struct file *, char *, size_t, loff_t *);
static ssize_t MD5_write(struct file *, const char *, size_t, loff_t *);



static struct file_operations md5_fops =
        {
                .owner = THIS_MODULE,
                .open = MD5_open_close,
                .read = MD5_read,
                .write = MD5_write,
                .release = MD5_open_close
        };






static int  MD5_open_close(struct inode* inodep, struct file * filep) {
    struct tty_listitem *tty_item;
    dev_t key;

    if (!current->signal->tty) {
        pr_err("md5: process \"%s\" has no ctl tty\n", current->comm);
        return -EINVAL;
    }
    key = tty_devnum(current->signal->tty);

    /* look for tty in the list */
    spin_lock(&tty_list_lock);
    tty_item = md5_lookfor_tty(key);
    spin_unlock(&tty_list_lock);

    if (!tty_item) /* no tty_item because kmalloc error */
        return -ENOMEM;

    filep->private_data = tty_item;

    return nonseekable_open(inodep, filep);

}


static ssize_t MD5_read(struct file * filep, char * buffer, size_t len, loff_t * offset) {
    struct tty_listitem *tty_item = filep->private_data;

    len = min(len, MD5_HASH_SIZE - tty_item->index);

    if (copy_to_user(buffer, tty_item->hash + tty_item->index, len))
        return -EFAULT;

    tty_item->index += len;

    return len;
}


static ssize_t MD5_write(struct file * filep, const char * buffer, size_t len, loff_t * offset) {
    struct tty_listitem *tty_item = filep->private_data;

    ssize_t err = md5sum((uint32_t*) tty_item->hash, buffer, len);

    tty_item->index = err ? tty_item->index : 0;

    return err ? err : len;
}


static int __init MD5_module_init(void) {

    struct device *MD5_device;
    int error;
    dev_t devt = 0;

    /* Get a range of minor numbers (starting with 0) to work with */
    error = alloc_chrdev_region(&devt, 0, 1, DEVICE_NAME);
    if (error < 0) {
        pr_err("Can't get major number\n");
        return error;
    }
    majorNumber = MAJOR(devt);
    pr_info("md5_char major number = %d\n", majorNumber);

    /* Create device class, visible in /sys/class */
    MD5charClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(MD5charClass)) {
        pr_err("Error creating sdma test module class.\n");
        unregister_chrdev_region(MKDEV(majorNumber, 0), 1);
        return PTR_ERR(MD5charClass);
    }

    /* Initialize the char device and tie a file_operations to it */
    cdev_init(&md5_cdev, &md5_fops);
    md5_cdev.owner = THIS_MODULE;
    /* Now make the device live for the users to access */
    cdev_add(&md5_cdev, devt, 1);

    MD5_device = device_create(MD5charClass,
                               NULL,   /* no parent device */
                               devt,    /* associated dev_t */
                               NULL,   /* no additional data */
                               DEVICE_NAME);  /* device name */

    if (IS_ERR(MD5_device)) {
        pr_err("Error creating sdma test class device.\n");
        class_destroy(MD5charClass);
        unregister_chrdev_region(devt, 1);
        return -1;
    }
    pr_info("md5 char module loaded\n");
    return 0;

    /*
    printk(KERN_INFO "MD5: Initializing the MD5_MODULE MD5M\n");

    // Try to dynamically allocate a major number for the device -- more difficult but worth it
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber<0){
        printk(KERN_ALERT "MD5 failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "MD5: registered correctly with major number %d\n", majorNumber);

    // Register the device class
    MD5charClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(MD5charClass)){                // Check for error and clean up if there is
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(MD5charClass);          // Correct way to return an error on a pointer
    }
    printk(KERN_INFO "MD5: device class registered correctly\n");

    // Register the device driver
    MD5charDevice = device_create(MD5charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(MD5charDevice)){               // Clean up if there is an error
        class_destroy(MD5charClass);           // Repeated code but the alternative is goto statements
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(MD5charDevice);
    }
    printk(KERN_INFO "MD5: device class created correctly\n"); // Made it! device was initialized
    return 0;
     */
}

static void __exit MD5_module_exit(void) {
    struct tty_listitem *lptr, *next;
    unregister_chrdev_region(MKDEV(majorNumber, 0), 1);
    device_destroy(MD5charClass, MKDEV(majorNumber, 0));
    cdev_del(&md5_cdev);
    class_destroy(MD5charClass);
    list_for_each_entry_safe(lptr, next, &tty_list, list)
    {
        list_del(&lptr->list);
        kfree(lptr);

        pr_info("dummy char module Unloaded\n");

        /*device_destroy(MD5charClass, MKDEV(majorNumber, 0));     // remove the device
        class_unregister(MD5charClass);                          // unregister the device class
        class_destroy(MD5charClass);                             // remove the device class
        unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
        printk(KERN_INFO "MD5: Goodbye from the MD5!\n");
         */
    }
}

module_init(MD5_module_init);
module_exit(MD5_module_exit);