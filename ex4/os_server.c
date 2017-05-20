#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>


#define RANDOM_DATA_FILE_PATH "/dev/urandom"
#define BUFF_SIZE 1024
#define LISTEN_CONNECTIONS 10



bool done = false;


void signal_handler(int signum){
	done = true;
}


void create_key(int KEYLEN, char *KEY_PATH){

	int rand_src_fd = -1, curr_read, curr_write, read_bytes_cnt = 0, written_bytes_cnt = 0, want_to_read, want_to_write;
	char random_data_buff[BUFF_SIZE];
	int key_fd  = -1;

	/* create new file or truncate an existing key */
	key_fd = open(KEY_PATH, O_WRONLY | O_TRUNC | O_CREAT, 0777);
	if(key_fd == -1){
		printf("ERROR creating / overwriting existing key file in %s:  %s\n", KEY_PATH, strerror(errno));
		exit(errno);
	}

	if((rand_src_fd = open(RANDOM_DATA_FILE_PATH, O_RDONLY)) < 0){
		printf("ERROR opening random data file %s: %s\n", RANDOM_DATA_FILE_PATH, strerror(errno));
		exit(errno);
	}

	while(read_bytes_cnt < KEYLEN){

		if(KEYLEN - read_bytes_cnt < BUFF_SIZE){
			want_to_read = KEYLEN - read_bytes_cnt;
		} else {
			want_to_read = BUFF_SIZE;
		}

		curr_read = read(rand_src_fd, random_data_buff, want_to_read);

		if(curr_read < 0){
			printf("ERROR read() from random data file %s: %s\n", RANDOM_DATA_FILE_PATH, strerror(errno));
			exit(errno);
		}

		else if(curr_read == 0){				/* we received EOF in random data file - I decided to define this situation as an error */
			printf("ERROR - random data file %s is too small for the key. \n", RANDOM_DATA_FILE_PATH);
			exit(-1);
		}

		read_bytes_cnt += curr_read;
		want_to_write = curr_read;

		while(written_bytes_cnt < want_to_write){

			curr_write = write(key_fd, random_data_buff + written_bytes_cnt, want_to_write - written_bytes_cnt);

			if(curr_write < 0){
				printf("ERROR write() to key file %s: %s\n", KEY_PATH, strerror(errno));
				exit(errno);
			}

			written_bytes_cnt += curr_write;
		}

		written_bytes_cnt = 0;
	}

	close(rand_src_fd);
	close(key_fd);
}


void check_key(char *KEY_PATH){
	struct stat key_stat;
	int key_size;


	if (stat(KEY_PATH, &key_stat) == 0) {       /* stat in order to check that key file size > 0 */

		key_size = key_stat.st_size;

		if(S_ISDIR(key_stat.st_mode)){
			printf("ERROR, '%s' is a directory, not a file.\n", KEY_PATH);
			exit(-1);
		}
		if(key_size == 0) {                  /* empty key file */
			printf("ERROR, Empty key file in %s. \n", KEY_PATH);
			exit(-1);
		}
	}

	else if (errno == ENOENT){
		printf("ERROR, key file in %s does not exist. \n", KEY_PATH);
		exit(errno);
	}
	else if (errno != EOVERFLOW) { 			/* ignore stat problem with big files, we just want to check that filesize > 0 */
		printf("Error getting stat of the key file: %s\n", strerror(errno));
		exit(errno);
	}

}



int main(int argc, char *argv[])
{
	short PORT;
	char *KEY_PATH, *string_end;
	int KEYLEN;
	int connfd = -1, listenfd = -1;
	struct sigaction new_sigint, old_sigint;
	new_sigint.sa_handler = signal_handler;
	new_sigint.sa_flags = 0;
	int key_fd = -1;

	if(argc != 3 && argc != 4){
		printf("ERROR - wrong number of arguments\n");
		return(-1);
	}

	/* PORT - the port the server listens for connections on (a positive short) */
	if((PORT = (short)strtol(argv[1], &string_end, 10)) <= 0 || *string_end){
		printf("ERROR - PORT has to be a positive short\n");
		return(-1);
	}

	/* KEY_PATH - path to the key file */
	KEY_PATH = argv[2];

	/* if KEYLEN provided */
	if(argc == 4){

	/* KEYLEN - the port the server listens for connections on (a positive short) */
	if((KEYLEN = (int)strtol(argv[3], &string_end, 10)) <= 0 || *string_end){
		printf("ERROR - KEYLEN has to be a positive integer\n");
		return(-1);
	}

	create_key(KEYLEN, KEY_PATH);
	}

	else{		/* KEYLEN is not provided - we need to check the key file in KEY_PATH exists and not empty */
		check_key(KEY_PATH);
	}

    struct sockaddr_in serv_addr = {0};

    /* Creating a TCP socket that listens on PORT */
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("ERROR creating socket: %s\n", strerror(errno));
		return(errno);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = any local machine address
	serv_addr.sin_port = htons(PORT);

	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))){
	   printf("ERROR, Bind Failed: %s \n", strerror(errno));
	   return(errno);
	}

	if(listen(listenfd, LISTEN_CONNECTIONS)){
	   printf("ERROR, Listen Failed. %s \n", strerror(errno));
	   return(errno);
	}

	/* Registering a signal handler for SIGINT */
	if(sigaction(SIGINT, &new_sigint, &old_sigint) < 0){
		printf("Signal handle registration for SIGINT failed. %s\n", strerror(errno));
		return(errno);
	}

	/* waiting for connections */

	while(!done){

		/* Accepting connection*/
		connfd = accept(listenfd, NULL, NULL);

		if(done) break;

		if(connfd<0){

			if(errno == EINTR) continue;		/* can be when other son finish - we want to ignore it, and go to the while beginning again */

			else{
				printf("ERROR Accepting a connection: %s \n", strerror(errno));
				return(errno);
			}
		}


		int forked_process_id = fork();

		if(forked_process_id < 0){
			printf("ERROR, fork failed: %s\n", strerror(errno));
			return(errno);
		}

		/* inside son process */
		else if(forked_process_id == 0){

			if(listenfd != -1) close(listenfd);

			key_fd = open(KEY_PATH, O_RDONLY);

			if(key_fd < 0){
				printf("ERROR opening key file in %s: %s\n", KEY_PATH, strerror(errno));
				return(errno);
			}

			char sendRecvBuff[BUFF_SIZE], keyBuff[BUFF_SIZE];
			int curr_read, numkey = 0, curr_key, i, curr_write, written_cnt = 0;

			while (!done) {

				/* first, init the buffer before reading */
				memset(sendRecvBuff, '0', sizeof(sendRecvBuff));

				/* Try to read BUFF_SIZE bytes from the client - Read data from the client until EOF. */
				curr_read = read(connfd, sendRecvBuff, BUFF_SIZE);

				if(curr_read < 0){
					printf("ERROR read() from client: %s\n", strerror(errno));
					return errno;
				}

				/* Received EOF - we're done (and that's the ONLY case we're done!) */
				else if (curr_read == 0) {
					break;
				}

				/* Whenever data is read, encrypt, and send back to the client */

				/* set number of bytes read from the key file to 0 */
				numkey = 0;

				/* iterate reading from key until reaching curr_read bytes */
				while (numkey < curr_read) {
					curr_key = read(key_fd, keyBuff + numkey, curr_read - numkey);

					if (curr_key < 0) {
						printf("error read() from key file: %s\n", strerror(errno));
						return errno;
					}

					/* reached end of key, reset and read from start */
					else if (curr_key == 0) {
						if (lseek(key_fd, SEEK_SET, 0) < 0) {
							printf("error lseek() key: %s\n", strerror(errno));
							return(errno);
						}
					}

					/* success - increase our counter of bytes read from key */
					else {
						numkey += curr_key;
					}
				}

				/* now we have curr_read bytes - from both input and key file */

				/* perform encryption operation */
				for (i = 0; i < curr_read; ++i){
					sendRecvBuff[i] = sendRecvBuff[i] ^ keyBuff[i];
				}

				written_cnt = 0;						 /* number of bytes we wrote to output*/

				while (written_cnt  < curr_read) {
					curr_write = write(connfd, sendRecvBuff + written_cnt, curr_read - written_cnt);
					if(curr_write < 0) {
						printf("Error write() to the client: %s\n", strerror(errno));
						return(errno);
					}

					written_cnt += curr_write;
				}

			}

			if(key_fd != -1){
				close(key_fd);
				key_fd = -1;
			}

			done = 1;		/* end of son process */
		}
		else{ 				/* we are in father process */
			if(connfd != -1) close(connfd);
			connfd = -1;

		}

	}

	if(connfd != -1) close(connfd);
	if(listenfd != -1) close(listenfd);

	//restore old sigint handler
	if(sigaction(SIGINT, &old_sigint, NULL) < 0) {
		printf("Error restoring the original signal handler for SIGINT. %s\n", strerror(errno));
		return(errno);
	}

	return 0;
}



