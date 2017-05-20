#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

#define MAJOR_NUM 245

#define IOCTL_SET_PID _IOW(MAJOR_NUM, 0, unsigned long)
#define IOCTL_SET_FD _IOW(MAJOR_NUM, 1, unsigned long)
#define IOCTL_CIPHER _IOW(MAJOR_NUM, 2, unsigned long)

#define DEVICE_FILE_NAME "kci_dev"
#define DEVICE_FILE "/dev/kci_dev"
#define SUCCESS 0
#define BUFF_SIZE 1024
#define DEVICE_RANGE_NAME "kci_kmod"
#define LOG_PATH "/sys/kernel/debug/kcikmod/calls"
#define LOG_FILENAME "calls"
#define LOG_BUFF_SIZE 1024
#define DEBUGFS_DIR "kcikmod"
#define DEBUGFS_FILE "calls"
#define LOG_MSG_MAX_LEN LOG_BUFF_SIZE/2



#endif
