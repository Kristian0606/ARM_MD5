//
// Created by jbardaghji on 25/05/23.
//
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/uaccess.h> /* copy_from/to_user */
/*
The specific value of LEN_OF_KERNEL_BUFFER will depend on various factors,
including the requirements of MD5 cryptocore,
the expected maximum size of the data to be processed,
and any constraints imposed by the underlying hardware or system
*/
#define MD5_HASH_SIZE 16
#define  DEVICE_NAME "MD5_module"
#define  CLASS_NAME  "MD5"

// *****************************************************************************
// *                            MD5 Implementation
// *                        source:
// *****************************************************************************
static uint32_t S[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static uint32_t K[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
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

#define ROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))

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

// Pre-processing: padding with zeros
//append "0" bit until message length in bit ≡ 448 (mod 512)
//append length mod (2 pow 64) to message
    new_len = ((((initial_len + 8) / 64) + 1) * 64) - 8;

/* allocate memory */
    msg = kmalloc(new_len + 64, GFP_KERNEL); // also appends "0" bits (we alloc also 64 extra bytes...)
    if (!msg)
        return -ENOMEM;
    memset(msg, 0, new_len + 64);

/* copy from user space to kernel space */
    if (copy_from_user(msg, buff, initial_len))
        return -EFAULT;
    //memcpy(msg, initial_msg, initial_len);

    msg[initial_len] = 128; // write the "1" bit

    bits_len = 8*initial_len; // note, we append the len
    memcpy(msg + new_len, &bits_len, 4); // in bits at the end of the buffer

// Process the message in successive 512-bit chunks:
//for each 512-bit chunk of message:
    for(offset=0; offset<new_len; offset += (512/8)) {

// break chunk into sixteen 32-bit words w[j], 0 ≤ j ≤ 15
    uint32_t *w = (uint32_t *) (msg + offset);

// Initialize hash value for this chunk:
    uint32_t a = hash[0];
    uint32_t b = hash[1];
    uint32_t c = hash[2];
    uint32_t d = hash[3];

// Main loop:
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

// Add this chunk's hash to result so far:
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
}

// cleanup
    kfree(msg);

    return 0;

}



static int    majorNumber;
static struct class*  MD5charClass  = NULL;
static struct device* MD5charDevice = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int     MD5_open_close(struct inode *, struct file *);
static ssize_t MD5_read(struct file *, char *, size_t, loff_t *);
static ssize_t MD5_write(struct file *, const char *, size_t, loff_t *);
static long int MD5_ioctl(struct file *, unsigned int , unsigned long );

static struct file_operations fops =
        {
                .owner = THIS_MODULE,
                .open = MD5_open_close,
                .read = MD5_read,
                .write = MD5_write,
                .release = MD5_open_close,
                .unlocked_ioctl = MD5_ioctl,
                .compat_ioctl = MD5_ioctl
        };


static long int MD5_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    /*
    switch (cmd) {
        case BLINK_RATE:
            selected_register = BLINK_RATE;
            break;
        case ENABLE:
            selected_register = ENABLE;
            break;
    }
    */

    printk(KERN_INFO "LM: Executing IOCTL\n");

    return -1;
}



/** @brief The device release function that is called whenever the device is initialized/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */

static int  MD5_open_close(struct inode* inodep, struct file * filep) {
    /*
    selected_register = ENABLE;
    WRITE_DATA_TO_THE_HW(0);
    selected_register = BLINK_RATE;
    WRITE_DATA_TO_THE_HW(0);
    */

    printk(KERN_INFO "LM: Executing OPENRELEASE\n");
    return 0;

}


/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */


static ssize_t MD5_read(struct file * filep, char * buffer, size_t len, loff_t * offset) {

    char kernel_data[LEN_OF_KERNEL_BUFFER]; // Define a kernel buffer to hold the data, it should be replaced later with other variable in the driver structure
    size_t data_length; // Variable to store the length of the data obtained

    // Perform necessary operations to obtain the data from the MD5 cryptocore
    // For example, retrieve computed MD5 hash or other relevant information

    // Copy the data from kernel space to user space
    /*
    if (copy_to_user(buffer, kernel_data, data_length))
    {
        return -EFAULT; // Error occurred while copying data to user space
    }

    // Return the number of bytes read
    return data_length;
    */

    /*int data;
    READ_DATA_FROM_THE_HW (&data);
    copy_to_user(buffer, &data, 4);*/
    printk(KERN_INFO "LM: Executing READ\n");
    return 4;
}


/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user.
 *  @param file A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */

/*
 * the data is copied from user space to a kernel buffer using the copy_from_user function to ensure safe memory access.
 * Afterward, we can process the data using the MD5 cryptocore,performing any necessary computations or data handling.
 */

static ssize_t MD5_write(struct file * filep, const char * buffer, size_t len, loff_t * offset) {
    // Validate and copy the user-provided data to kernel space
    /*char kernel_buffer[LEN_OF_KERNEL_BUFFER];
    if (copy_from_user(kernel_buffer, buffer, len))
    {
        return -EFAULT; // Error occurred while copying data from user space
    }

    // Process the data using the MD5 cryptocore
    // Perform necessary computations or data handling

    // Return the number of bytes written
    return len;*/

    //WRITE_DATA_TO_THE_HW( buffer);
    printk(KERN_INFO "LM: Executing WRITE\n");
    return 1;
}




// Function to initialize the MD5 cryptocore device driver
static int __init md5_cryptocore_driver_init(void)
{

    // Perform necessary initialization specific to QEMU here
    // For example, set up virtual memory-mapped interfaces or QEMU-specific configurations,

    // Allocate any required resources (e.g., memory, IRQ, etc.)
    // Make sure to handle errors and free resources in case of failure

    // Register the driver with the operating system
    // For example, create a character device, set up file operations, etc.
    // Make sure to handle errors and clean up if registration fails
    printk(KERN_INFO "MD5: Initializing the MD5_MODULE LKM\n");

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
    printk(KERN_INFO "MD5 Cryptocore Driver Initialized\n"); // Made it! device was initialized
    return 0;
}

// Function to clean up and unregister the MD5 cryptocore device driver
static void __exit md5_cryptocore_driver_exit(void)
{
    // Release any resources allocated during initialization
    // For example, free memory, release IRQ, etc.

    // Unregister the driver from the operating system
    // For example, unregister the character device, clean up file operations, etc.
    device_destroy(MD5charClass, MKDEV(majorNumber, 0));     // remove the device
    class_unregister(MD5charClass);                          // unregister the device class
    class_destroy(MD5charClass);                             // remove the device class
    unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
    printk(KERN_INFO "MD5: Goodbye from the LKM!\n");

    // Print a message to indicate successful exit
    printk(KERN_INFO "MD5 Cryptocore Driver Exited\n");
}

// Register the initialization and exit functions with the kernel
module_init(md5_cryptocore_driver_init);
module_exit(md5_cryptocore_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jack....");
MODULE_DESCRIPTION("MD5 Cryptocore Device Driver");
