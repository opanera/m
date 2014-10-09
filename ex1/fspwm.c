#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/platform_device.h>

#include <plat/gpio-cfg.h>

#include "ioctl.h"


#define DEVICE_MAJOR 250
#define DEVICE_MINOR 0
#define DEVICE_NUM   1
#define DEVICE_NAME  "fspwm"

#define GPD_BASE     0xE0300080
#define PWM_BASE     0xEA000000

struct gpioreg {
	int __iomem *gpdcon;
};

struct pwmreg {
	int __iomem *tcfg0;
	int __iomem *tcfg1;
	int __iomem *tcon;
	int __iomem *tcntb;
	int __iomem *tcmpb;
};

struct fsdev {
	struct cdev cdev;
	struct gpioreg gpioreg;
	struct pwmreg pwmreg;
	struct platform_device *pdev;
};
static struct fsdev *fspwm;

static int fspwm_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int nr;
	struct fsdev *dev;
	struct platform_device *pdev;

	dev = container_of(inode->i_cdev, struct fsdev, cdev);
	filp->private_data = dev;
	pdev = dev->pdev;

	if (!request_mem_region(GPD_BASE, 4, "fspwmcon")) {
		dev_err(&pdev->dev, "request memory region failure\n");
		return -EIO;
	}

	if (!request_mem_region(PWM_BASE, 12, "fspwmcon")) {
		dev_err(&pdev->dev, "request memory region failure\n");
		return -EIO;
	}

	nr = *((int *)(pdev->dev.platform_data));
	if (!request_mem_region(PWM_BASE + 12 + nr * 12, 12, "fspwmtmr")) {
		dev_err(&pdev->dev, "request memory region failure\n");
		return -EIO;
	}

	dev->gpioreg.gpdcon= ioremap(GPD_BASE, 4);
	if (!dev->gpioreg.gpdcon) {
		dev_err(&pdev->dev, "ioremap gpdcon failure\n");
		ret = -EINVAL;
	}

	dev->pwmreg.tcfg0 = ioremap(PWM_BASE, 12);
	if (!dev->pwmreg.tcfg0) {
		dev_err(&pdev->dev, "ioremap tcfg0 failure\n");
		ret = -EINVAL;
	}
	dev->pwmreg.tcfg1 = dev->pwmreg.tcfg0 + 1;
	dev->pwmreg.tcon = dev->pwmreg.tcfg0 + 2;

	dev->pwmreg.tcntb = ioremap(PWM_BASE + 12 + nr * 12, 12);
	if (!dev->pwmreg.tcntb) {
		dev_err(&pdev->dev, "ioremap tcntb failure\n");
		ret = -EINVAL;
	}
	dev->pwmreg.tcmpb = dev->pwmreg.tcntb + 1;

	iowrite32((ioread32(dev->gpioreg.gpdcon) & ~(0xF << nr * 4)) | (0x2 << nr * 4), dev->gpioreg.gpdcon);

	/* Initialize pwm timer1 */
	// Set prescaler for timer 1, 0xf
	iowrite32(ioread32(dev->pwmreg.tcfg0) & ~0xFF, dev->pwmreg.tcfg0);	
	// Set divider mux for timer 1, 1/8
	iowrite32((ioread32(dev->pwmreg.tcfg1) & ~(0xF << nr * 4)) | (0x2 << nr * 4), dev->pwmreg.tcfg1);
	iowrite32(6000, dev->pwmreg.tcntb);
	iowrite32(3000, dev->pwmreg.tcmpb);
	// Timer1: Stop; Manual Update : 1; Output Inverter On; Auto Reload On
	iowrite32((ioread32(dev->pwmreg.tcon) & ~(0xF << 8)) | (0xE << 8), dev->pwmreg.tcon);


	return ret;
}

static int fspwm_release(struct inode *inode, struct file *filp)
{
	struct fsdev *dev = filp->private_data;
	int nr = *((int *)(dev->pdev->dev.platform_data));

	iowrite32(ioread32(dev->gpioreg.gpdcon) & ~(0xF << nr * 4), dev->gpioreg.gpdcon);
	iounmap(dev->gpioreg.gpdcon);
	iounmap(dev->pwmreg.tcfg0);
	iounmap(dev->pwmreg.tcntb);

	release_mem_region(GPD_BASE, 4);
	release_mem_region(PWM_BASE, 12);
	release_mem_region(PWM_BASE + 12 + nr * 12, 12);

	return 0;
}

static int fspwm_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct fsdev *dev = filp->private_data;

	switch (cmd) {
		case BEEP_ON:
			// Start Timer1
			iowrite32((ioread32(dev->pwmreg.tcon) & ~(0xF<<8)) | (0xD << 8), dev->pwmreg.tcon);
			break;
		case BEEP_OFF:
			// Stop Timer1
			iowrite32(ioread32(dev->pwmreg.tcon) & ~(0x1<<8), dev->pwmreg.tcon);
			break;
		case SET_CNT:
			iowrite32(arg, dev->pwmreg.tcntb);
			iowrite32(arg / 2, dev->pwmreg.tcmpb);
			break;
		case SET_PRE:
			iowrite32((ioread32(dev->pwmreg.tcfg0) & ~(0xFF)) | (arg & 0xFF), dev->pwmreg.tcfg0);
			break;
		default:
			break;
	}
	return 0;
}

static struct file_operations fspwm_fops = {
	.owner = THIS_MODULE,
	.open = fspwm_open,
	.release = fspwm_release,
	.ioctl = fspwm_ioctl,
};

int fspwm_probe(struct platform_device *pdev)
{
	int ret;
	dev_t devno;

	fspwm = kmalloc(sizeof(struct fsdev), GFP_KERNEL);
	if (!fspwm) {
		dev_err(&pdev->dev, "kmalloc failure\n");
		return -ENOMEM;
	}

	devno = MKDEV(DEVICE_MAJOR, DEVICE_MINOR);
	ret = register_chrdev_region(devno, DEVICE_NUM, DEVICE_NAME);

	if(ret) {
		dev_err(&pdev->dev, "register char device region failed\n");
		goto out_kfree;
	}

	cdev_init(&fspwm->cdev, &fspwm_fops);
	fspwm->cdev.owner = THIS_MODULE;

	ret = cdev_add(&fspwm->cdev, devno, DEVICE_NUM);
	if (ret < 0) {
		dev_err(&pdev->dev, "add cdev failure\n");
		goto out_ureg_region;
	}

	fspwm->pdev = pdev;

	return 0;

out_ureg_region:
	unregister_chrdev_region(devno, DEVICE_NUM);
out_kfree:
	kfree(fspwm);

	return ret;
}

int fspwm_remove(struct platform_device *pdev)
{
	dev_t devno;

	cdev_del(&fspwm->cdev);
	devno = MKDEV(DEVICE_MAJOR, DEVICE_MINOR);
	unregister_chrdev_region(devno, DEVICE_NUM);

	return 0;
}

static struct platform_driver fspwm_drv = {
	.probe = fspwm_probe,
	.remove = fspwm_remove,
	.driver = {
		.name = "fspwm",
		.owner = THIS_MODULE,
	},
};

static int __init fspwm_init(void)
{
	return platform_driver_register(&fspwm_drv);
}

static void __exit fspwm_exit(void)
{
	platform_driver_unregister(&fspwm_drv);
}

module_init(fspwm_init);
module_exit(fspwm_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("fspwm sample");
