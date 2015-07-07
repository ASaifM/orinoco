#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Aya Saif El-yazal Mahfouz");

static int first_module_init(void)
{
	printk(KERN_DEBUG "Hello World!\n");
	return 0;

}

static void first_module_exit(void)
{
	
}

module_init(first_module_init);
module_exit(first_module_exit);
