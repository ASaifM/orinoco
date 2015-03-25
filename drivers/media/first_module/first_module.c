#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <media/media-device.h>
#include <media/media-entity.h>

#define DEVICE_NAME "Dummy"
#define FIRST_PADS_NUM 2
#define FIRST_PAD_SINK 0
#define FIRST_PAD_SOURCE 1

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Aya Saif El-yazal Mahfouz");

static int first_create_graph(struct media_device *mdev){
	struct media_entity *mentity;
	struct media_pad *mpad;
	int i, ret;
	
	ret = 0;
	for (i=0; i<7; i++) {
		mentity = kzalloc(sizeof(struct media_entity), GFP_KERNEL);
		if (mentity == NULL)
			return -ENOMEM; 
		mpad = kzalloc(FIRST_PADS_NUM * sizeof(struct media_pad), GFP_KERNEL);
		if (mpad == NULL)
                        return -ENOMEM;
		ret = media_entity_init(mentity, FIRST_PADS_NUM, mpad, 0);
		if (ret)
			return ret;
		ret = media_device_register_entity(mdev, mentity);
		if (ret)
			return ret; 
		mentity = NULL;
		mpad = NULL;
	}
	return ret;	
}

static int first_probe(struct platform_device *pdev)
{
	int ret = 0;
	/*struct media_device *mdev;
	
	
	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (mdev == NULL)
		return -ENOMEM;
	ret = media_device_register(mdev);
	if (ret)
		return ret;
	ret = first_create_graph(mdev);*/
	return ret;
};

static int first_remove(struct platform_device *pdev)
{	
	int ret = 0;
	/*struct media_device *mdev;

	mdev = platform_get_drvdata(pdev);
	media_device_unregister(mdev);
	*/
	return ret;
};


static struct platform_driver first_pdrv = {
	.probe = first_probe,
	.remove = first_remove,
	.driver = {
		.name = DEVICE_NAME,
	},	
};

static void first_dev_release(struct device *dev)
{}

static struct platform_device first_pdev = {
	.name           = DEVICE_NAME,
	.dev.release    =  first_dev_release,
};


static struct media_device mdev =
{		
	.dev = &first_pdev.dev,
	.model = "Dummy",
};

static int first_module_init(void)
{
	int ret = 0;

	printk(KERN_DEBUG "Hello World!\n");
	ret = platform_device_register(&first_pdev);
	printk(KERN_DEBUG "platform device registered %d \n", ret);
	if (ret)
		return ret;
	ret = platform_driver_register(&first_pdrv);
	printk(KERN_DEBUG "platform registeration status %d \n", ret);
	if (ret)
		return ret;
	ret = media_device_register(&mdev);
	return ret ;

}

static void first_module_exit(void)
{	
	media_device_unregister(&mdev);
	platform_driver_unregister(&first_pdrv);
	platform_device_unregister(&first_pdev);	
}

module_init(first_module_init);
module_exit(first_module_exit);
