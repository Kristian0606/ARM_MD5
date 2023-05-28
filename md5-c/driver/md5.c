//
// Created by jbardaghji on 25/05/23.
//
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/cdev.h>
#include <linux/fs.h> /* everything... */


#include <linux/init.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h> /* kmalloc() */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */
#include <linux/tty.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/sched/signal.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack....");
MODULE_DESCRIPTION("MD5 Crypto core Device Driver");

#define  DEVICE_NAME "md5"
#define  CLASS_NAME  "md5"
#define MD5_DEV_MODE  0666
#define MD5_FIRST_MINOR 0

#define MD5_HASH_SIZE 16

/*
The specific value of LEN_OF_KERNEL_BUFFER will depend on various factors,
including the requirements of MD5 cryptocore,
the expected maximum size of the data to be processed,
and any constraints imposed by the underlying hardware or system
*/


// *****************************************************************************
// *                            MD5 Implementation
// *                        source:
// *****************************************************************************
static const uint32_t r[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

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

#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

ssize_t md5sum(uint32_t *hash,const char __user *buff,size_t initial_len){
    uint8_t *msg = NULL;
    uint32_t temp, bits_len;
    int new_len, offset;

    hash[0] = 0x67452301;
    hash[1] = 0xefcdab89;
    hash[2] = 0x98badcfe;
    hash[3] = 0x10325476;

// Pre-processing: adding a single 1 bit
//append "1" bit to message
/* Notice: the input bytes are considered as bits strings,
   where the first bit is the most significant bit of the byte.[37] */
    new_len = ((((initial_len + 8) / 64) + 1) * 64) - 8;

/* allocate memory */
    msg = kmalloc(new_len + 64, GFP_KERNEL); // also appends "0" bits (we alloc also 64 extra bytes...)
    if (!msg)
        return -ENOMEM;
    memset(msg, 0, new_len + 64);

/* copy from user space to kernel space */
    if (copy_from_user(msg, buff, initial_len))
        return -EFAULT;

    msg[initial_len] = 128; // write the "1" bit

    bits_len = 8*initial_len; // note, we append the len
    memcpy(msg + new_len, &bits_len, 4); // in bits at the end of the buffer

    for(offset=0; offset<new_len; offset += (512/8)) {

    uint32_t *w = (uint32_t *) (msg + offset);


    uint32_t a = hash[0];
    uint32_t b = hash[1];
    uint32_t c = hash[2];
    uint32_t d = hash[3];

    uint32_t i;
    for(i = 0; i<64; i++) {
    uint32_t f, g;

        if (i < 16) {
            f = (b & c) | ((~b) & d);
            g = i;
        }else if (i < 32) {
            f = (d & b) | ((~d) & c);
            g = (5*i + 1) % 16;
        }else if (i < 48) {
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
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
}

// cleanup
    kfree(msg);

    return 0;

}
// *****************************************************************************
// *                           END OF MD5 Implementation
// *****************************************************************************


struct tty_listitem {
    dev_t key;                      ///< tty key
    uint8_t hash[MD5_HASH_SIZE];    ///< Message-Digest buffer
    size_t index;                   ///< Displacement on hash buffer
    struct list_head list;          ///< Pointer to the list head
};

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


static int md5_major = -1;
static int md5_minor = MD5_FIRST_MINOR;
struct class *md5_class = NULL;

static struct cdev  md5_cdev;
static dev_t dev_num;

// The prototype functions for the character driver -- must come before the struct definition
static int     MD5_open_close(struct inode *, struct file *);
static ssize_t MD5_read(struct file *, char *, size_t, loff_t *);
static ssize_t MD5_write(struct file *, const char *, size_t, loff_t *);

static const struct file_operations md5_fops =
        {
                .owner = THIS_MODULE,
                .open = MD5_open_close,
                .read = MD5_read,
                .write = MD5_write,
                .llseek = no_llseek
        };


static int MD5_open_close(struct inode *inode, struct file *filep) {
    struct tty_listitem *tty_item;
    dev_t key;

    // Check if the current process has a control tty
    if (!current->signal->tty) {
        pr_err("MD5 process \"%s\" has no ctl tty\n", current->comm);
        return -EINVAL;
    }
    // Get the dev_t key for the current process's control tty
    key = tty_devnum(current->signal->tty);

    /* look for tty in the list */
    spin_lock(&tty_list_lock);
    tty_item = md5_lookfor_tty(key);
    spin_unlock(&tty_list_lock);

    if (!tty_item) { /* no tty_item because kmalloc error */
        return -ENOMEM;
    }
    // Store the tty_item pointer in the file's private_data field
    filep->private_data = tty_item;
    return nonseekable_open(inode, filep);
}


static ssize_t MD5_read(struct file *filp, char __user *ubuff, size_t count, loff_t *f_pos) {
    struct tty_listitem *tty_item = filp->private_data;
    count = min(count, MD5_HASH_SIZE - tty_item->index);
    if (copy_to_user(ubuff, tty_item->hash + tty_item->index, count)){
        return -EFAULT;
    }
    tty_item->index += count;
    return count;
}



static ssize_t MD5_write(struct file *filp, const char __user *ubuff, size_t count, loff_t *f_pos) {

    struct tty_listitem *tty_item = filp->private_data;
    ssize_t err = md5sum((uint32_t*) tty_item->hash, ubuff, count);
    tty_item->index = err ? tty_item->index : 0;
    return err ? err : count;
}

static int md5_dev_uevent(struct device *dev, struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", MD5_DEV_MODE);
    return 0;
}


/* Function to initialize the MD5 crypto core character device driver
1.We will create a device number using alloc_chrdev_region().
2.We are making a device registration with cdev strucutres.
3.Creating device files (class and device).
 */
static int __init md5_cryptocore_driver_init(void) {

    dev_t first_dev = 0;
    int err;
    /* allocate driver region */
    if (alloc_chrdev_region(&first_dev, md5_minor, 1, DEVICE_NAME) < 0) {
        pr_err("MD5: unable to allocate '%s' region\n", DEVICE_NAME);
        goto error;
    }
    md5_major = MAJOR(first_dev);
    dev_num = first_dev;
    printk("MD5: %d major assigned\n", md5_major);

    /* create driver class */
    if ((md5_class = class_create(THIS_MODULE, CLASS_NAME)) == NULL) {
        printk("MD5: unable to create '%s' class\n", CLASS_NAME);
        goto error;
    } else
        md5_class->dev_uevent = md5_dev_uevent;

    /* setup md5 cdev */
    cdev_init(&md5_cdev, &md5_fops);
    if ((err = cdev_add(&md5_cdev, dev_num, 1))) {
        printk("MD5: error %d adding md5\n", err);
        return 0;
    }
    if (device_create(md5_class, NULL, dev_num, NULL, DEVICE_NAME) == NULL) {
        printk("MD5: md5 device node creation failed\n");
        cdev_del(&md5_cdev);
        return 0;
    }
    error:
    return -1;
}
/* Function to clean up and unregister the MD5 cryptocore device driver
 1.Unregister the driver from the operating system, deleting cdev.
 2. unregister the character device region, clean up file operations, etc
 */
static void __exit md5_cryptocore_driver_exit(void) {
    struct tty_listitem *lptr, *next;

    if (md5_class) {
        device_destroy(md5_class,dev_num);
        cdev_del(&md5_cdev);
        list_for_each_entry_safe(lptr, next, &tty_list, list)
        {
            list_del(&lptr->list);
            kfree(lptr);
        }
        class_destroy(md5_class);
    }
    if (md5_major != -1) {
        unregister_chrdev_region(MKDEV(md5_major, md5_minor),1);
        printk("md5: released major %d \n", md5_major);
    }
    printk("MD5: Driver unloaded\n");
}

// Register the initialization and exit functions with the kernel
module_init(md5_cryptocore_driver_init);
module_exit(md5_cryptocore_driver_exit);
