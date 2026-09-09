#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include "motor_ioctl.h"

#define GPIO_BASE 905

#define MIO_LEFT_FWD    0
#define MIO_LEFT_BWD    1
#define MIO_RIGHT_FWD   2
#define MIO_RIGHT_BWD   3

#define GPIO_LEFT_FWD   (GPIO_BASE + MIO_LEFT_FWD)
#define GPIO_LEFT_BWD   (GPIO_BASE + MIO_LEFT_BWD)
#define GPIO_RIGHT_FWD  (GPIO_BASE + MIO_RIGHT_FWD)
#define GPIO_RIGHT_BWD  (GPIO_BASE + MIO_RIGHT_BWD)

static dev_t dev_num;
static struct cdev motor_cdev;
static struct class* motor_class;

static void motor_set_dir(int dir)
{
    switch (dir) {
    case MOTOR_STOP:
        gpio_set_value(GPIO_LEFT_FWD, 0);
        gpio_set_value(GPIO_LEFT_BWD, 0);
        gpio_set_value(GPIO_RIGHT_FWD, 0);
        gpio_set_value(GPIO_RIGHT_BWD, 0);
        break;
    case MOTOR_FORWARD:
        gpio_set_value(GPIO_LEFT_FWD, 1);
        gpio_set_value(GPIO_LEFT_BWD, 0);
        gpio_set_value(GPIO_RIGHT_FWD, 1);
        gpio_set_value(GPIO_RIGHT_BWD, 0);
        break;
    case MOTOR_BACKWARD:
        gpio_set_value(GPIO_LEFT_FWD, 0);
        gpio_set_value(GPIO_LEFT_BWD, 1);
        gpio_set_value(GPIO_RIGHT_FWD, 0);
        gpio_set_value(GPIO_RIGHT_BWD, 1);
        break;
    case MOTOR_LEFT:
        gpio_set_value(GPIO_LEFT_FWD, 0);
        gpio_set_value(GPIO_LEFT_BWD, 1);
        gpio_set_value(GPIO_RIGHT_FWD, 1);
        gpio_set_value(GPIO_RIGHT_BWD, 0);
        break;
    case MOTOR_RIGHT:
        gpio_set_value(GPIO_LEFT_FWD, 1);
        gpio_set_value(GPIO_LEFT_BWD, 0);
        gpio_set_value(GPIO_RIGHT_FWD, 0);
        gpio_set_value(GPIO_RIGHT_BWD, 1);
        break;
    }
}

static int motor_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "motor_driver: open\n");
    return 0;
}

static int motor_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "motor_driver: release\n");
    return 0;
}

static long motor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int dir;
    switch (cmd) {
    case MOTOR_SET_DIR:
        dir = (int)arg;
        if(dir < MOTOR_STOP || dir > MOTOR_RIGHT) { return -EINVAL; }
        motor_set_dir(dir);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = motor_open,
    .release        = motor_release,
    .unlocked_ioctl = motor_ioctl,
};

static int request_output_pin(int gpio, const char *label)
{
    int ret = gpio_request(gpio, label);
    if(ret) {
        printk(KERN_ERR "motor_driver: gpio_request(%d,%s) 실패, ret=%d\n", gpio, label, ret);
        return ret;
    }
    gpio_direction_output(gpio, 0);
    return 0;
}

static int __init motor_driver_init(void)
{
    int ret;
    ret = request_output_pin(GPIO_LEFT_FWD, "motor_left_fwd");
    if(ret) { return ret; }

    ret = request_output_pin(GPIO_LEFT_BWD, "motor_left_bwd");
    if(ret) { goto err_free_left_fwd; }

    ret = request_output_pin(GPIO_RIGHT_FWD, "motor_right_fwd");
    if(ret) { goto err_free_left_bwd; }

    ret = request_output_pin(GPIO_RIGHT_BWD, "motor_right_bwd");
    if(ret) { goto err_free_right_fwd; }

    ret = alloc_chrdev_region(&dev_num, 0, 1, MOTOR_DEVICE_NAME);
    if(ret < 0) {
        printk(KERN_ERR "motor_driver: 장치 번호 할당 실패\n");
        goto err_free_right_bwd;
    }

    cdev_init(&motor_cdev, &fops);
    ret = cdev_add(&motor_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "motor_driver: 캐릭터 장치 추가 실패\n");
        goto err_unregister_chrdev;
    }

    motor_class = class_create(THIS_MODULE, MOTOR_DEVICE_NAME);
    if (IS_ERR(motor_class)) {
        printk(KERN_ERR "motor_driver: class 생성 실패\n");
        ret = PTR_ERR(motor_class);
        goto err_cdev_del;
    }

    device_create(motor_class, NULL, dev_num, NULL, MOTOR_DEVICE_NAME);
    printk(KERN_INFO "motor_driver: 초기화 완료\n");
    return 0;

err_cdev_del:
    cdev_del(&motor_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
err_free_right_bwd:
    gpio_free(GPIO_RIGHT_BWD);
err_free_right_fwd:
    gpio_free(GPIO_RIGHT_FWD);
err_free_left_bwd:
    gpio_free(GPIO_LEFT_BWD);
err_free_left_fwd:
    gpio_free(GPIO_LEFT_FWD);
    return ret;
}

static void __exit motor_driver_exit(void)
{
    motor_set_dir(MOTOR_STOP);

    device_destroy(motor_class, dev_num);
    class_destroy(motor_class);
    cdev_del(&motor_cdev);
    unregister_chrdev_region(dev_num, 1);

    gpio_free(GPIO_LEFT_FWD);
    gpio_free(GPIO_LEFT_BWD);
    gpio_free(GPIO_RIGHT_FWD);
    gpio_free(GPIO_RIGHT_BWD);

    printk(KERN_INFO "motor_driver: 종료\n");
}

module_init(motor_driver_init);
module_exit(motor_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YGC");
MODULE_DESCRIPTION("Boat motor direction control driver (ioctl, PS MIO GPIO)");