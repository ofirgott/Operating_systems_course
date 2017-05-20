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

#define FILEPATH "/tmp/mmapped.bin"
#define KILL_READER_PROCESS if(kill(RPID, SIGKILL) < 0)printf("Error sending SIGKILL signal to the reader process: %s\n", strerror(errno)); //we will not return errno because we want to return the original errno

int main(int argc, char *argv[]) {

	if (argc != 3) {
		printf("Error - wrong number of arguments.\n");
		return -1;
	}

	int NUM, RPID, fd, i, a_counter = 0;
	char *arr, *string_end;

	//Time measurement structures
	struct timeval t1, t2;
	double elapsed_millisec;

	struct sigaction old_sigterm, new_sigterm;
	new_sigterm.sa_handler = SIG_IGN; //ignore
	new_sigterm.sa_flags = 0;

	if ((RPID = strtol(argv[2], &string_end, 10)) <= 0 || *string_end) { //process ID of an already running mmap_reader (positive int)
		printf("Error - RPID has to be positive integer. \n");
		return -1;
	}

	if ((NUM = strtol(argv[1], &string_end, 10)) <= 0 || *string_end) { //number of bytes to transfer (positive integer)
		printf("Error - NUM has to be positive integer. \n");
		KILL_READER_PROCESS
		return -1;
	}

//ignore sigterm
	if (sigaction(SIGTERM, &new_sigterm, &old_sigterm) < 0) {
		printf("Signal handle registration for SIGTERM failed. %s\n",
				strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	fd = open(FILEPATH, O_RDWR | O_CREAT);

	if (fd == -1) {
		printf("Error opening file for writing: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	if (chmod(FILEPATH, 0600) < 0) {           //set the file permission to 0600
		printf("Error setting file permissions: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	// Force the file to be of the same size as the (mmapped) array

	if (lseek(fd, NUM - 1, SEEK_SET) == -1) {
		printf("Error calling lseek() to 'stretch' the file: %s\n",
				strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	// Something has to be written at the end of the file,
	// so the file actually has the new size.

	if (write(fd, "", 1) != 1) {
		printf("Error writing last byte of the file: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	//Now the file is ready to be mmapped.
	arr = (char*) mmap(NULL, NUM, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

	if (arr == MAP_FAILED) {
		printf("Error mmapping the file: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	//start tune measuring
	if (gettimeofday(&t1, NULL) != 0) {
		printf("Error getting start time: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	// now write to the file as if it were memory
	for (i = 0; i < NUM - 1; ++i) {
		arr[i] = 'a';
		a_counter++;
	}

	arr[NUM - 1] = '\0';
	a_counter++;

	if (kill(RPID, SIGUSR1) < 0) {
		printf("Error sending SIGUSR1 signal: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	//finish time measuring
	if (gettimeofday(&t2, NULL) != 0) {
		printf("Error getting finish time: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	// Counting time elapsed
	elapsed_millisec = (t2.tv_sec - t1.tv_sec) * 1000.0;
	elapsed_millisec += (t2.tv_usec - t1.tv_usec) / 1000.0;

	// Final report
	printf("%d were written in %f milliseconds through MMAP\n", a_counter,
			elapsed_millisec);

	//free the mmapped memory
	// this also ensures the changes commit to the file
	if (munmap(arr, NUM) == -1) {
		printf("Error un-mmapping the file: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	// un-mmaping doesn't close the file, so we still need to do that.
	if (close(fd) == -1) {
		printf("Error closing the file: %s\n", strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}

	if (sigaction(SIGTERM, &old_sigterm, NULL) < 0) {
		printf("Error restoring the original signal handler for SIGTERM. %s\n",
				strerror(errno));
		KILL_READER_PROCESS
		return errno;
	}
	return 0;

}
