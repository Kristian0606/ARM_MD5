//
// Created by jbardaghji on 25/05/23.
//

// this file is for testing a simple module

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

static int hello_init(void) {
    printk("<1> Hello world!\n");
    return 0;
}
static void hello_exit(void) {
    printk("<1> Bye, cruel world\n");
}


module_init(hello_init);
module_exit(hello_exit);