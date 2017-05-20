#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdbool.h>

#define FILEPATH "/tmp/mmapped.bin"

struct sigaction old_sigterm;

//----------------------------------------------------------------------------
// Signal handler for sigusr1.
void signal_handler(int signum) {
	int fd, fileSize, a_counter = 0, i = 0;
	struct stat fileStat;
	char *arr;
	bool null_char_ok = true;

	//Time measurement structures
	struct timeval t1, t2;
	double elapsed_millisec;

	fd = open(FILEPATH, O_RDONLY);      //open in read only modde

	if (fd == -1) {
		printf("Error opening file for reading: %s\n", strerror(errno));
		exit (errno);
	}

	if (stat(FILEPATH, &fileStat) < 0) {  //stat in order to check the file size
		printf("Error getting stat of the file: %s\n", strerror(errno));
		exit (errno);
	}

	fileSize = fileStat.st_size;

	//start tune measuring
	if (gettimeofday(&t1, NULL) != 0) {
		printf("Error getting start time: %s\n", strerror(errno));
		exit (errno);
	}

	//Now the file is ready to be mmapped.
	arr = (char*) mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);

	if (arr == MAP_FAILED) {
		printf("Error mmapping the file: %s\n", strerror(errno));
		exit (errno);
	}

	while (i < fileSize && arr[i] != '\0') {
		if (arr[i] == 'a')
			a_counter++;
		i++;
	}

	if (arr[i] != '\0') {
		printf("Error, No null char in file. \n");
		null_char_ok = false;
	} else
		a_counter++;

	//finish time measuring
	if (gettimeofday(&t2, NULL) != 0) {
		printf("Error getting finish time: %s\n", strerror(errno));
		exit (errno);
	}

	// Counting time elapsed
	elapsed_millisec = (t2.tv_sec - t1.tv_sec) * 1000.0;
	elapsed_millisec += (t2.tv_usec - t1.tv_usec) / 1000.0;

	// Final report
	printf("%d were read in %f milliseconds through MMAP\n", a_counter,
			elapsed_millisec);

	if (!null_char_ok)
		exit(-1);

	//free the mmapped memory
	if (munmap(arr, fileSize) == -1) {
		printf("Error un-mmapping the file: %s\n", strerror(errno));
		exit (errno);
	}

	// un-mmaping doesn't close the file, so we still need to do that.
	if (close(fd) == -1) {
		printf("Error closing the file: %s\n", strerror(errno));
		exit (errno);
	}

	//delete the file
	if (unlink(FILEPATH) < 0) {
		printf("Error deleting the file: %s\n", strerror(errno));
		exit (errno);
	}

	if (sigaction(SIGTERM, &old_sigterm, NULL) < 0) {
		printf("Error restoring the original signal handler for SIGTERM. %s\n",
				strerror(errno));
		exit (errno);
	}

	exit (EXIT_SUCCESS);

}

//----------------------------------------------------------------------------
// The flow:
// 1. register USR1 signal handler
// 2. enter an infinite loop, sleeping 2 seconds in each iteration
int main(void) {
	// Structure to pass to the registration syscall
	struct sigaction new_action, new_sigterm;
	// Assign pointer to our handler function
	new_action.sa_handler = signal_handler;
	new_sigterm.sa_handler = SIG_IGN; //ignore
	// Remove any special flag
	new_action.sa_flags = 0;
	new_sigterm.sa_flags = 0;
	// Register the handlers

	//ignore sigterm
	if (sigaction(SIGTERM, &new_sigterm, &old_sigterm) < 0) {
		printf("Signal handle registration for SIGTERM failed. %s\n",
				strerror(errno));
		return errno;
	}

	if (sigaction(SIGUSR1, &new_action, NULL) < 0) {
		printf("Signal handle registration for SIGUSR1 failed. %s\n",
				strerror(errno));
		return errno;

	}

	// Meditate until killed
	while (1) {
		sleep(2);
	}

	return 0;
}

