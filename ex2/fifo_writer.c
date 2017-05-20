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

#define WRITE_BUFFER_SIZE 1024
#define FIFO_PATH "/tmp/osfifo"

int fd, written_bytes_cnt = 0;
struct sigaction old_sigint;

//Time measurement structures
struct timeval t1, t2;
double elapsed_millisec;

void finish_time_print_exit() {

	//finish time measuring
	if (gettimeofday(&t2, NULL) != 0) {
		printf("Error getting finish time: %s\n", strerror(errno));
		exit (errno);
	}

	// Counting time elapsed
	elapsed_millisec = (t2.tv_sec - t1.tv_sec) * 1000.0;
	elapsed_millisec += (t2.tv_usec - t1.tv_usec) / 1000.0;

	// Final report
	printf("%d were written in %f milliseconds through FIFO\n",
			written_bytes_cnt, elapsed_millisec);

	if (unlink(FIFO_PATH) < 0) {
		printf("Error unlink the FIFO file: %s\n", strerror(errno));
		exit (errno);
	}

	if (close(fd) == -1) {
		printf("Error closing the file: %s\n", strerror(errno));
		exit (errno);
	}

	if (sigaction(SIGTERM, &old_sigint, NULL) < 0) {
		printf("Error restoring the original signal handler for SIGINT. %s\n",
				strerror(errno));
		exit (errno);
	}
}

void sigpipe_handler(int signum) {

	printf("SIGPIPE signal sent because of writing to closed pipe: %s\n",strerror(EPIPE));
	finish_time_print_exit();
	exit (EPIPE);

}

int main(int argc, char *argv[]) {

	if (argc != 2) {
		printf("Error - wrong number of arguments.\n");
		return -1;
	}

	int NUM, fd, write_cnt_remain, bytes_to_write, curr_written_bytes;
	struct stat fifoStat;
    char write_buf[WRITE_BUFFER_SIZE] = {[0 ... WRITE_BUFFER_SIZE-1] = 'a'};
    struct sigaction new_sigint, act_pipe;
    char *string_end;

    new_sigint.sa_handler = SIG_IGN; //ignore
    new_sigint.sa_flags = 0;

    if ((NUM = strtol(argv[1], &string_end, 10)) <= 0 || *string_end) { //number of bytes to transfer (positive integer)
        printf("Error - NUM has to be positive integer. \n");
        return -1;
    }

    //ignore sigterm
    if(sigaction(SIGINT, &new_sigint, &old_sigint) < 0) {
        printf("Signal handle registration for SIGINT failed. %s\n", strerror(errno));
        return errno;
    }

    //check if fifo file exists
    if(stat(FIFO_PATH, &fifoStat) < 0) {
        if(errno != ENOENT) {     // file exists, but we have problem with stat
            printf("Error getting stat of fifo file: %s\n", strerror(errno));
            return errno;
        }
        else {                    // file does not exists
            if(mkfifo(FIFO_PATH, 0600) < 0) {        //create fifo file
                printf("Error creating fifo file: %s\n", strerror(errno));
                return errno;
            }
        }
    }
    else {                   //stat succeed
        if(!S_ISFIFO(fifoStat.st_mode)) { //file exists, but it in not a fifo file
            printf("Error - file in %s exists, but it is not a fifo file. \n", FIFO_PATH);
            return -1;
        }
        else {               //fifo file already exists - make sure permission is OK
            if(chmod(FIFO_PATH, 0600) < 0) {
                printf("Error setting existing fifo file permissions: %s\n", strerror(errno));
                return errno;
            }
        }
    }

    //open the fifo file for writing
    if((fd = open(FIFO_PATH, O_WRONLY)) < 0) {
        printf("Error opening file: %s\n", strerror(errno));
        return errno;
    }

    act_pipe.sa_handler = sigpipe_handler;

    if(sigaction(SIGPIPE, &act_pipe, NULL) < 0) {
        printf("Signal handle registration for SIGPIPE failed. %s\n", strerror(errno));
        return errno;
    }

    //start tune measuring
    if (gettimeofday(&t1, NULL) != 0) {
        printf("Error getting start time: %s\n", strerror(errno));
        return errno;
    }

    //write NUM 'a' bytes to the named pipe file
    write_cnt_remain = NUM;
    while(write_cnt_remain > 0) {

        if(write_cnt_remain > WRITE_BUFFER_SIZE) bytes_to_write = WRITE_BUFFER_SIZE;
        else bytes_to_write = write_cnt_remain;

        if((curr_written_bytes = write(fd, write_buf, bytes_to_write)) < 0 && errno != EPIPE) { //writing problem, not because of pipe
            printf("Error writing to fifo file: %s\n", strerror(errno));
            return errno;
        }

        written_bytes_cnt += curr_written_bytes;
        write_cnt_remain -= curr_written_bytes;

    }

    finish_time_print_exit();

    return 0;
    }

