#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>

#define READ_BUFFER_SIZE 1024
#define FIFO_PATH "/tmp/osfifo"

int main(void) {

	int i, fd, read_cnt = 0, curr_read, read_a = 0;
	struct stat fifoStat;
	char buffer[READ_BUFFER_SIZE];
	struct sigaction old_sigint, new_sigint;

	//Time measurement structures
	struct timeval t1, t2;
	double elapsed_millisec;

	//ignore sigterm
	if (sigaction(SIGINT, &new_sigint, &old_sigint) < 0) {
		printf("Signal handle registration for SIGINT failed. %s\n",
				strerror(errno));
		return errno;
	}

	//first stat to check if file exists
	if (stat(FIFO_PATH, &fifoStat) < 0) {
		if (errno == ENOENT)
			sleep(2);           //wait only 2 more seconds for the file
		else {                              //file exists, but problem with stat
			printf("Error getting stat of fifo file: %s\n", strerror(errno));
			return errno;
		}
	}

	fd = open(FIFO_PATH, O_RDONLY);      //open in read only mode

	if (fd == -1) {
		printf("Error opening file for reading: %s\n", strerror(errno));
		return errno;
	}

	//check the stat of the file opened, to make sure it is a fifo file
	if (stat(FIFO_PATH, &fifoStat) < 0) {
		printf("Error getting stat of fifo file: %s\n", strerror(errno));
		return errno;
	} else if (!S_ISFIFO(fifoStat.st_mode)) {
		printf("Error, the file in %s is not a fifo file", FIFO_PATH);
		return -1;
	}

	//fifo file is OK and opened

	//start time measuring
	if (gettimeofday(&t1, NULL) != 0) {
		printf("Error getting start time: %s\n", strerror(errno));
		return errno;
	}

	while ((curr_read = read(fd, buffer, READ_BUFFER_SIZE)) > 0) {
		for (i = 0; i < curr_read; i++) {
			if (buffer[i] == 'a')
				read_a++;
		}
	}

	if (curr_read < 0) {
		printf("Error reading from file: %s\n", strerror(errno));
		return errno;
	}

	//finish time measuring
	if (gettimeofday(&t2, NULL) != 0) {
		printf("Error getting finish time: %s\n", strerror(errno));
		return errno;
	}

	// Counting time elapsed
	elapsed_millisec = (t2.tv_sec - t1.tv_sec) * 1000.0;
	elapsed_millisec += (t2.tv_usec - t1.tv_usec) / 1000.0;

	// Final report
	printf("%d were read in %f milliseconds through FIFO\n", read_a,
			elapsed_millisec);

	if (close(fd) == -1) {
		printf("Error closing the file: %s\n", strerror(errno));
		return errno;
	}

	if (sigaction(SIGTERM, &old_sigint, NULL) < 0) {
		printf("Error restoring the original signal handler for SIGINT. %s\n",
				strerror(errno));
		return (errno);
	}

	return 0;

}
