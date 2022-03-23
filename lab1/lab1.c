#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <stdbool.h>
 
 
static int dev_open(struct inode *i, struct file *f);
static int dev_close(struct inode *i, struct file *f);
static ssize_t dev_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos);
static ssize_t dev_write(struct file *f, const char __user *ubuf, size_t len, loff_t *ppos);
 
static ssize_t proc_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos);
static ssize_t proc_write(struct file *f, const char __user *ubuf, size_t len, loff_t *ppos);
 
#define DRIVER_NAME "lab_1_device"
#define DRIVER_CLASS "lab_1_driver_class"
#define DEV_NAME "lab_1"
#define PROC_FILENAME "VAR4"
#define BUF_SIZE 200
#define MAX_SIZE 100
#define MAX_NUM_SIZE 9
 
MODULE_LICENSE("GPL");
 
static dev_t first; 
static struct cdev c_dev; 
static struct class *cl;
static struct proc_dir_entry *entry;
static uint16_t seq_counter = 0;
static uint32_t sequence[100];
static uint32_t tab_space_counter = 0;
 
 
static int dev_open(struct inode *i, struct file *f) {
	return 0;
}
 
static int dev_close(struct inode *i, struct file *f) {
	return 0;
}
 
static void auxiliary_read(char *buffer, uint32_t * const length) {
	char *buffer_ptr = buffer;
	uint32_t i;
	for(i = 0; i < seq_counter; i ++) {
		buffer_ptr += sprintf(buffer_ptr,"%d ", sequence[i]);
	}
	*length = buffer_ptr - buffer; 
}
 
static ssize_t dev_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos) {
	char buf[BUF_SIZE];
	uint32_t length;
	auxiliary_read(buf, &length);
 
	if(*ppos > 0 || len < length) {
		return 0;
	}
 
	if(copy_to_user(ubuf, buf, length) != 0) {
		return -1;
	}
 
	printk(KERN_INFO "%s", buf);
	*ppos += length;
	return length;	
}
 
 
static ssize_t proc_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos) {
	char buf[BUF_SIZE];
	uint32_t length;
	auxiliary_read(buf, &length);
 
	if(*ppos > 0 || len < length) {
		return 0;
	}
 
	if(copy_to_user(ubuf, buf, length) != 0) {
		return -1;
	}
	*ppos += length;
	return length;
}
 
static ssize_t dev_write(struct file *f, const char __user *ubuf, size_t len, loff_t *ppos) {
	char buf[BUF_SIZE];
	char num1[MAX_NUM_SIZE] = {0};
	char num2[MAX_NUM_SIZE] = {0};
	char op = 0;
 
	uint16_t space_counter = 0;
 
	if(*ppos > 0 || len > BUF_SIZE) {
		return 0;
	}
	if(copy_from_user(buf, ubuf, len) != 0) {
		return -1;
	}
 
	uint32_t i = 0;
 
  while (1)
  {
    if (i >= MAX_NUM_SIZE) {
      printk(KERN_ERR "lab 1: Number 1 is too long");
      return -EINVAL;
    }
    if (i == 0 && buf[i] == '-') {
      num1[i] = buf[i];
    } else if (buf[i] >= '0' && buf[i] <= '9') {
      num1[i] = buf[i];
    } else if (buf[i] == '+' || buf[i] == '-' || buf[i] == '/' || buf[i] == '*') {
      op = buf[i];
      i += 1;
      break;
    } else {
      printk(KERN_ERR "lab 1: 125 Can't parse input");
      return -EINVAL;
    }
    i += 1;
  }
 
 
  uint32_t buf_num2_start_index = i;
  while (1)
  {
    if (!buf[i]) {
      if (i == buf_num2_start_index) {
        printk(KERN_ERR "lab 1: 136 Can't parse input");
        return -EINVAL;
      }
      break;
    } else if (i >= MAX_NUM_SIZE) {
      printk(KERN_ERR "lab 1: Number 2 is too long");
      return -EINVAL;
    } else if (buf[i] >= '0' && buf[i] <= '9') {
      num2[i - buf_num2_start_index] = buf[i];
    } else {
      break;
    } 
    i += 1;
  }
 
	if(seq_counter >= MAX_SIZE) {
		return -1;
	}
 
  long left_num;
  long right_num;
 
  if (kstrtol(num1, 10, &left_num) || kstrtol(num2, 10, &right_num)) {
      printk(KERN_ERR "lab 1: 160 Can't parse input");
      return -EINVAL;
  }
 
  switch (op)
  {
  case '+':
    sequence[seq_counter++] = left_num + right_num;
    break;
  case '-':
    sequence[seq_counter++] = left_num - right_num;
    break;
  case '/':
    if (right_num == 0) {
      printk(KERN_ERR "lab 1: division by zero isn't allowed");
      return -EINVAL;
    }
    sequence[seq_counter++] = left_num / right_num;
    break;
  case '*':
    sequence[seq_counter++] = left_num * right_num;
    break;
  default:
    printk(KERN_ERR "lab 1: 181 Can't parse input");
    return -EINVAL;
  }
 
	size_t length;
	length = strlen(buf);
	*ppos += length;
	return length;
}
 
 
static ssize_t proc_write(struct file *f, const char __user *ubuf, size_t len, loff_t *ppos) {
	return 0;
}
 
static struct file_operations dev_ops = {
	.open = dev_open,
	.release = dev_close,
	.read = dev_read,
	.write = dev_write
};
 
 
static struct file_operations f_ops = {
  .owner = THIS_MODULE,
  .read = proc_read,
  .write = proc_write
};
 
static int __init chr_driver_init(void) { 
	if(alloc_chrdev_region(&first, 0, 1, DRIVER_NAME) < 0) {
		return -1;
	}
 
 
 
	if((cl = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		unregister_chrdev_region(first, 1);
		return -1;
	}
 
	if(device_create(cl, NULL, first, NULL, DEV_NAME) == NULL) {
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
		return -1;
	}
 
 
 
	cdev_init(&c_dev, &dev_ops);
 
 
	if(cdev_add(&c_dev, first, 1) == -1) {
		device_destroy(cl, first);
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
		return -1;
	}
 
 
	if((entry = proc_create(PROC_FILENAME, 0777, NULL, &f_ops)) == NULL) {
		cdev_del(&c_dev);
		device_destroy(cl, first);
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
	}
	return 0;
}
 
static void __exit chr_driver_exit(void) {
	cdev_del(&c_dev);
	device_destroy(cl, first);
	class_destroy(cl);
	unregister_chrdev_region(first, 1);
	proc_remove(entry);
}
 
module_init(chr_driver_init);
module_exit(chr_driver_exit);
