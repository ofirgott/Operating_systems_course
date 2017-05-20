#undef __KERNEL__
#define __KERNEL__ /* We're part of the kernel */
#undef MODULE
#define MODULE     /* Not a permanent part, though. */

#include "kci.h"
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <asm/uaccess.h>    /* for get_user and put_user */
#include <linux/string.h>   /* for memset*/
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/paravirt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>

MODULE_LICENSE("GPL v2");

/* Declarations */
static struct dentry *file;
static struct dentry *subdir;
unsigned long **sys_call_table;
unsigned long original_cr0;

static char log_buf[LOG_BUFF_SIZE] = {0};
static size_t buf_pos;

static int pid;
static int fd;
static int encryption_flag;

/* Original syscalls */
asmlinkage long (*ref_read)(unsigned int read_fd, char* __user buf, size_t count);
asmlinkage long (*ref_write)(unsigned int write_fd, char* __user buf, size_t count);

int write_to_log(char *func, int fd, int count, char *op_type);



int change_buffer(char* __user buf, int count, int change_int){

	int i;
	char ch;

	for (i = 0; i < count; i++) {

		if(get_user(ch, buf + i) == -EFAULT) return -1;

		ch += change_int;

		if(put_user(ch, buf + i) == -EFAULT) return -1;
	}

	return SUCCESS;
}


asmlinkage long new_read(unsigned int read_fd, char* __user buf, size_t count){

	int need_to_decrypt = 0, read_bytes, rc = 0;

	if(encryption_flag == 1 && read_fd == fd && pid == current->pid) need_to_decrypt = 1;
	else need_to_decrypt = 0;

	if(need_to_decrypt){
		if(write_to_log("Read", fd, count, "Try to") < 0) return -1;
	}

	read_bytes = ref_read(read_fd, buf, count);

	if(need_to_decrypt && read_bytes >= 0){			// we have data to decrypt

		rc = change_buffer(buf, read_bytes, -1);

		if(rc < 0){
			printk("kci_kmod - Error accessing user memory in read function. \n");
			return -1;
		}

		if(write_to_log("Read", fd, read_bytes, "Succeed to") < 0) return -1;

	}
	return read_bytes;
}




asmlinkage long new_write(unsigned int write_fd, char* __user buf, size_t count){

	int need_to_encrypt = 0, wrote_bytes, rc = 0;

	if(encryption_flag == 1 && write_fd == fd && pid == current->pid) need_to_encrypt = 1;
	else need_to_encrypt = 0;

	if(need_to_encrypt){

		write_cr0(original_cr0 & ~0x00010000);			/*to deal with read only buffer case */

		if(write_to_log("Write", fd, count, "Try to") < 0) return -1;

		rc = change_buffer(buf, count, 1);
		if(rc < 0){
			printk("kci_kmod - Error accessing user memory in write function. \n");
			return -1;
		}

		write_cr0(original_cr0);

	}

	wrote_bytes = ref_write(write_fd, buf, count);

	if(wrote_bytes < 0) return wrote_bytes;

	if(need_to_encrypt){				/* if we encrypted the data, we need to decrypt it after writing */

		write_cr0(original_cr0 & ~0x00010000);			/* to deal with read only buffer case */

		rc = change_buffer(buf, count, -1);
		if(rc < 0){
			printk("kci_kmod - Error accessing user memory in write function. \n");
			return -1;
		}

		write_cr0(original_cr0);

		if(write_to_log("Write", fd, wrote_bytes, "Succeed to") < 0) return -1;

	}

	return wrote_bytes;
}

static unsigned long **aquire_sys_call_table(void)
{
	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;

	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;

		if (sct[__NR_close] == (unsigned long *) sys_close)
			return sct;

		offset += sizeof(void *);
	}

	return NULL;
}



long device_ioctl(struct file* file, unsigned int ioctl_num, unsigned long  ioctl_param){

  /* Switch according to the ioctl called */
  if(IOCTL_SET_PID == ioctl_num){
	  pid = (int) ioctl_param;
	  printk("kci_kmod - setting process ID to %d\n", pid);
  }
  else if(IOCTL_SET_FD == ioctl_num){
	  fd = (int) ioctl_param;
	  printk("kci_kmod - setting file descriptor to %d\n", fd);
  }
  else if(IOCTL_CIPHER == ioctl_num){
	  encryption_flag = (int) ioctl_param;
	  printk("kci_kmod - setting encryption_flag to %d\n", encryption_flag);
  }
  else return -1;

  return 0;

}
const struct file_operations fops = {
		.unlocked_ioctl = device_ioctl,
};


int write_to_log(char *func, int fd, int count, char *op_type){		//op_type: "Try to" or "Succeed to"


	int ret = 0;
	char log_msg[LOG_MSG_MAX_LEN];

	ret = sprintf(log_msg, "PID = %d  |  FD = %d  | %s %s %d bytes. \n", pid, fd, op_type, func, count);

	if(ret < -1){
		printk("kci_kmod - Error writing to the private log file. \n");
		return -1;
	}

	if(buf_pos + ret >= LOG_BUFF_SIZE){
		memset(log_buf, 0, LOG_BUFF_SIZE);
		buf_pos = 0;
	}

	strncpy(log_buf + buf_pos, log_msg, ret);
	buf_pos += ret;

	pr_debug("%s", log_msg);

	return SUCCESS;

}


static ssize_t device_read(struct file *filp, char *buffer, size_t len, loff_t *offset){

	return simple_read_from_buffer(buffer, len, offset, log_buf, buf_pos);

}

const struct file_operations log_fops = {
	.owner = THIS_MODULE,
	.read = device_read,
};


static int __init kcikmod_init(void){

	int rc = 0;

	/* Creating a private log file */

	printk("*** Initializing kci_kmod module *** \n");

	printk("Creating log file for kci_kmod in debugfs. \n");

	subdir = debugfs_create_dir(DEBUGFS_DIR, NULL);

	if(IS_ERR(subdir)){
		printk("Error creating directory for kci_kmod log file. \n");
		return PTR_ERR(subdir);
	}

	if(!subdir){
		printk("Error creating directory for kci_kmod log file. \n");
		return -ENOENT;
	}

	file = debugfs_create_file(DEBUGFS_FILE, S_IRUSR, subdir, NULL, &log_fops);

	if (!file) {
		printk("Error creating log file for kci_kmod. \n");
		debugfs_remove_recursive(subdir);
		return -ENOENT;
	}

	printk("kci_kmod log file successfully created in /sys/kernel/debug/%s/%s.\n", DEBUGFS_DIR, DEBUGFS_FILE);

	pid = -1;
	fd = -1;
	encryption_flag = 0;
	buf_pos = 0;

	/* Intercepting the read and write system call */

	if(!(sys_call_table = aquire_sys_call_table())){
		printk("Error acquiring syscall table pointer \n");
		return -1;
	}

	printk("Intercepting WRITE & READ syscalls\n");

	original_cr0 = read_cr0();
	write_cr0(original_cr0 & ~0x00010000);

	ref_read = (void *)sys_call_table[__NR_read];
	ref_write = (void *)sys_call_table[__NR_write];

	sys_call_table[__NR_read] = (unsigned long *)new_read;
	sys_call_table[__NR_write] = (unsigned long *)new_write;

	write_cr0(original_cr0);

	/* Registering device */

	rc = register_chrdev(MAJOR_NUM, DEVICE_FILE, &fops);
	if(rc < 0){
		printk("Error registering kci_kmod device \n");
		return -1;
	}
	printk("kci_kmod successfully initialized\n");

	return SUCCESS;
}

static void __exit kcikmod_exit(void){

	debugfs_remove_recursive(subdir);					/* deleting the log file */
	printk("kci_kmod log file in /sys/kernel/debug/%s/%s succsesfully deleted.\n", DEBUGFS_DIR, DEBUGFS_FILE);

	if(!sys_call_table) return;

	printk("Restoring WRITE and READ syscalls\n");
	write_cr0(original_cr0 & ~0x00010000);
	sys_call_table[__NR_read] = (unsigned long *)ref_read;
	sys_call_table[__NR_write] = (unsigned long *)ref_write;
	write_cr0(original_cr0);

	msleep(2000);

	unregister_chrdev(MAJOR_NUM, DEVICE_FILE);			/* unregistering device */
	printk("*** kci_kmod module successfully uninstalled *** \n");
}



module_init(kcikmod_init);
module_exit(kcikmod_exit);
