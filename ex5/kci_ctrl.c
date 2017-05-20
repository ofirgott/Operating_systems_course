
#include "kci.h"
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/stat.h>


/* open the device file and send the specific ioctl command and argument */
void send_ioctl(int command, int arg){

	int fd;

	fd = open(DEVICE_FILE, 0);
	if(fd < 0){
		printf("Error opening device file in %s: %s \n", DEVICE_FILE, strerror(errno));
		exit(errno);
	}

	if(ioctl(fd, command, arg) < 0){
		printf("Error in ioctl function: %s",strerror(errno));
		exit(errno);
	}

	if(close(fd) == -1){
		printf("Error closing device file: %s\n", strerror(errno));
		exit(errno);
	}

}

int copy_log_file(){

	int in_fd, out_fd;
	char buff[BUFF_SIZE];
	ssize_t n_read, curr_write, numdst;

	in_fd = open(LOG_PATH, O_RDONLY);
	if(in_fd < 0){
		printf("Error in copying log file - open log file in %s: %s\n", LOG_PATH, strerror(errno));
		exit(errno);
	}

	out_fd = open(LOG_FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0777);
	if(out_fd < 0){
		printf("Error creating / overwriting copied log file: %s\n", strerror(errno));
		exit(errno);
	}


	while(1) {
	     n_read = read(in_fd, buff, sizeof(buff));
	     if(n_read < 0){
	    	 printf("Error in copying log file - error read() from input file %s: %s\n", LOG_PATH, strerror(errno));
	    	 exit(errno);
	     }

	     else if(n_read==0){
	    	 break;
	     }

	     //number of bytes we wrote to output
	     numdst = 0;

	     while(numdst < n_read){
	    	 curr_write = write(out_fd, buff + numdst, n_read - numdst);
	    	 if(curr_write < 0){
	    		 printf("Error in copying log file - error write() to output file: %s\n", strerror(errno));
	    		 exit(errno);
	    	 }
	    	 numdst += curr_write;
	     }
	}

	if(close(in_fd) == -1){
		printf("Error in copying log file -  close input log file: %s\n", strerror(errno));
		exit(errno);
	}

	if(close(out_fd) == -1){
		printf("Error in copying log file -  close output log file: %s\n", strerror(errno));
		exit(errno);
	}

	return 0;
}

int main(int argc, char** argv){

	int fd, rc, pid;

	if(argc != 2 && argc != 3){
		printf("Error, wrong number of arguments\n");
		return -1;
	}

	if(strcmp(argv[1], "-init") == 0){

		/* load module */
		fd = open(argv[2], O_RDONLY);
		if (fd < 0) {
			printf("Error opening kernel object file in %s: %s\n", argv[2], strerror(errno));
			return errno;
		}

		rc = syscall(__NR_finit_module, fd, "", 0);
		if(rc < 0){
			printf("Error in finit_module(): %s\n", strerror(errno));
			return errno;
		}

		rc = mknod(DEVICE_FILE, 0777 | S_IFCHR, makedev(MAJOR_NUM, 0));
		if(rc < 0){
			printf("Error in mknod() for %s: %s\n", DEVICE_FILE, strerror(errno));
			return errno;
		}

		if(close(fd) == -1){
			printf("Error closing kernel object file: %s\n", strerror(errno));
			return errno;
		}
	}

	else if(strcmp(argv[1], "-pid") == 0){

		pid = (int) strtol(argv[2], NULL, 10);
		send_ioctl(IOCTL_SET_PID, pid);
	}

	else if(strcmp(argv[1], "-fd") == 0){

		fd = (int) strtol(argv[2], NULL, 10);
		send_ioctl(IOCTL_SET_FD, fd);
	}

	else if(strcmp(argv[1], "-start") == 0){
		send_ioctl(IOCTL_CIPHER, 1);
	}

	else if(strcmp(argv[1], "-stop") == 0){
		send_ioctl(IOCTL_CIPHER, 0);
	}

	else if(strcmp(argv[1], "-rm") == 0){

		/* copies the log file into a file in the current directory with the same name */
		if(copy_log_file()){
			printf("Error coping log file\n");
			return -1;
		}

		/* remove the kernel module from the kernel space */
		rc = syscall(__NR_delete_module, DEVICE_RANGE_NAME, O_NONBLOCK);
		if(rc < 0){
			printf("Error deleting %s module: %s", DEVICE_FILE_NAME, strerror(errno));
			return errno;
		}

		/* Finally, it removes the device file created by init */
		if(unlink(DEVICE_FILE) < 0){
			printf("Error deleting the device file in %s: %s\n", DEVICE_FILE, strerror(errno));
			return errno;
		}

	}

	else{
		printf("Error - invalid arguments \n");
		return -1;
	}


	return 0;


}
