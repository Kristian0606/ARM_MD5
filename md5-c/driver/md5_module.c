//
// Created by jbardaghji on 25/05/23.
//
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>



#define  DEVICE_NAME "MD5_module"
#define  CLASS_NAME  "MD5"


static int    majorNumber;
static struct class*  MD5charClass  = NULL;
static struct device* MD5charDevice = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int     MD5_open_close(struct inode *, struct file *);
static ssize_t MD5_read(struct file *, char *, size_t, loff_t *);
static ssize_t MD5_write(struct file *, const char *, size_t, loff_t *);
static long int MD5_ioctl(struct file *, unsigned int , unsigned long );

// Function to initialize the MD5 cryptocore device driver
static int md5_cryptocore_driver_init(void)
{
    // Perform necessary initialization specific to QEMU here
    // For example, set up virtual memory-mapped interfaces or QEMU-specific configurations

    // Allocate any required resources (e.g., memory, IRQ, etc.)
    // Make sure to handle errors and free resources in case of failure

    // Register the driver with the operating system
    // For example, create a character device, set up file operations, etc.
    // Make sure to handle errors and clean up if registration fails

    // Print a message to indicate successful initialization
    printk(KERN_INFO "MD5 Cryptocore Driver Initialized\n");

    return 0; // Return success
}

// Function to clean up and unregister the MD5 cryptocore device driver
static void md5_cryptocore_driver_exit(void)
{
    // Release any resources allocated during initialization
    // For example, free memory, release IRQ, etc.

    // Unregister the driver from the operating system
    // For example, unregister the character device, clean up file operations, etc.

    // Print a message to indicate successful exit
    printk(KERN_INFO "MD5 Cryptocore Driver Exited\n");
}

// Register the initialization and exit functions with the kernel
module_init(md5_cryptocore_driver_init);
module_exit(md5_cryptocore_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack....");
MODULE_DESCRIPTION("MD5 Cryptocore Device Driver");
