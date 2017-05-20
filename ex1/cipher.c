#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // for open flags
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 1024
#define PATH_MAX_SIZE 1024

int main(int argc, char** argv) {

	if(argc != 4){
        printf("wrong number of arguments");
        return -1;
	}

	char *input_dir, *keyPath, *output_dir;
	int i, output_curr_fd;
	struct dirent *input_dp;
	DIR *input_dfd;
	DIR *output_dfd;
	struct stat keyStat;
	off_t keySize;
	char inputBuf[BUF_SIZE], keyBuf[BUF_SIZE], outputBuf[BUF_SIZE];
	char curr_input_filename[PATH_MAX_SIZE], curr_output_filename[PATH_MAX_SIZE];
	ssize_t len_input, len_key, keyBytesCounter;

	input_dir = argv[1];
	keyPath = argv[2];
	output_dir = argv[3];

	//open the input directory

	if ((input_dfd = opendir(input_dir)) == NULL) {
		printf("Error in opening the input directory: %s\n", strerror(errno));
		return errno;
	}

	if (strcmp(input_dir, output_dir) == 0) {
		printf("Error, The output directory is not different from the input directory. \n");
		return -1;
	}

	//check the key file
	int key_fd = open(keyPath, O_RDONLY);        //open for read only

	if (key_fd < 0) {
		printf("Error opening file: %s\n", strerror(errno));
		return errno;
	}

	if (stat(keyPath, &keyStat) == 0) {       //stat in order to check that key file size > 0

		keySize = keyStat.st_size;

		if (keySize == 0) {                  //empty file
			printf("Empty key file, size 0.\n");
			return -1;
		}

	}

	else if (errno != EOVERFLOW) { //ignore stat problem with big files, we just want to check that filesize > 0
		printf("Error getting stat of the key file: %s\n", strerror(errno));
		return errno;
	}

	//open the output directory

	if ((output_dfd = opendir(output_dir)) == NULL) {

		if ((errno == ENOENT) && (mkdir(output_dir, 0777) < 0)) {
			printf("Error creating the output directory: %s\n",	strerror(errno));
		}

		else if (errno != ENOENT || (output_dfd = opendir(output_dir)) == NULL) {
			printf("Error in opening the output directory: %s\n", strerror(errno));
			return errno;
		}
	}

	// for each file in the input directory

	while ((input_dp = readdir(input_dfd)) != NULL) {

		// full path to file
		sprintf(curr_input_filename, "%s/%s", input_dir, input_dp->d_name);


		// skip '.' and '..'
		if (strcmp("..", input_dp->d_name) != 0 && strcmp(".", input_dp->d_name) != 0) {

			int input_curr_fd = open(curr_input_filename, O_RDONLY);

			if (input_curr_fd < 0) {
				printf("Error opening file: %s\n", strerror(errno));
				return errno;
			}

			sprintf(curr_output_filename, "%s/%s", output_dir, input_dp->d_name);

			if ((output_curr_fd = open(curr_output_filename, O_RDWR | O_TRUNC | O_CREAT, 0777)) == -1) {
				printf("error creating / overwriting existing file %s\n", strerror(errno));
				return errno;
			}

			if (lseek(key_fd, 0, SEEK_SET) == -1) {
						printf("Error seeking key file: %s\n", strerror(errno));
						return errno;
            }

			while (((len_input = read(input_curr_fd, inputBuf, BUF_SIZE)) > 0)) { //number of bytes read from current input file



				len_key = read(key_fd, keyBuf, len_input);

				if (len_key < 0) {
					printf("Error reading from key file: %s\n",	strerror(errno));
					return errno;
				}

				keyBytesCounter = len_key;

				while (keyBytesCounter < len_input) {   //filling the key buffer

					if (lseek(key_fd, 0, SEEK_SET) == -1) {
						printf("Error seeking key file: %s\n", strerror(errno));
						return errno;
					}

					len_key = read(key_fd, keyBuf + keyBytesCounter, len_input - keyBytesCounter);

					if (len_key < 0) {
						printf("Error reading from key file: %s\n", strerror(errno));
						return errno;
					}

					keyBytesCounter += len_key;
				}

				for (i = 0; i < len_input; i++) {
					outputBuf[i] = inputBuf[i] ^ keyBuf[i];
				}

				if ((write(output_curr_fd, outputBuf, len_input)) == -1) {
					printf("error writing to output file %s\n",	strerror(errno));
					return errno;
				}

			}

			if (len_input < 0) {
				printf("Error reading from input file: %s\n", strerror(errno));
				return errno;
			}

			if(close(output_curr_fd) == -1){
				printf("Error closing output file: %s\n", strerror(errno));
				return errno;
			}

			if(close(input_curr_fd) == -1){
				printf("Error closing input file: %s\n", strerror(errno));
				return errno;
			}


		}
	}

	if(close(key_fd) == -1){
		printf("Error closing key file: %s\n", strerror(errno));
		return errno;
	}

	if(closedir(input_dfd) == -1){
		printf("Error closing input directory: %s\n", strerror(errno));
		return errno;
	}
	if(closedir(output_dfd) == -1){
		printf("Error closing output directory: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

