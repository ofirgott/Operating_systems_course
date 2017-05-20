#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#define BUFF_SIZE 	1024


int main(int argc, char *argv[])
{
    int sockfd = 0, in_fd, out_fd;
    short PORT;

    char sendRecvBuff[BUFF_SIZE];
    char *IP, *string_end;
	char *input_filename, *output_filename;

    struct sockaddr_in serv_addr = {0};

	if(argc != 5){
		printf("ERROR - wrong number of arguments\n");
		return -1;
	}

	/* the IP address of the server (as a string, IPv4) */
	IP = argv[1];

	/* the port the server listens on (a positive short) */
	if((PORT = (short)strtol(argv[2], &string_end, 10)) <= 0 || *string_end){
		printf("ERROR - PORT has to be positive short\n");
		return -1;
	}

	/* the input file to read input from */
	input_filename = argv[3];

	/* the output file to write the output to */
	output_filename = argv[4];

	in_fd = open(input_filename, O_RDONLY);        /* open for read only */
	if (in_fd < 0) {
		printf("ERROR opening input file: %s\n", strerror(errno));
		return errno;
	}

	out_fd = open(output_filename, O_WRONLY | O_TRUNC | O_CREAT, 0777);
	if(out_fd == -1){
		printf("ERROR creating / overwriting existing output file %s\n", strerror(errno));
		return errno;
	}

	/* creating the socket */
	 if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("ERROR creating socket: %s\n", strerror(errno));
		return errno;
	}

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT); //htons for endiannes
    serv_addr.sin_addr.s_addr = inet_addr(IP);

    /* connects socket to the above address */
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
       printf("ERROR Connecting to the socket: %s\n", strerror(errno));
       return errno;
    }

    int numsrc; //current reading from src file
    int curr_sent;
    int sent_bytes_cnt = 0; //total counter
    int recv_bytes_cnt = 0;
    int curr_read = 0; //curr read from the server
    int notwritten;
    int curr_write;
    int written_bytes_cnt = 0;


    while (1) {
		/* first, init the buffer before reading */
    	memset(sendRecvBuff, '0', sizeof(sendRecvBuff));

    	/* try to read BUFF_SIZE bytes from the input file */
		numsrc = read(in_fd, sendRecvBuff, BUFF_SIZE);
		if (numsrc < 0) {
			printf("ERROR read() from input file %s: %s\n", input_filename, strerror(errno));
			exit(errno);
		}

		/* received EOF - we're done */
		else if (numsrc == 0) {
			break;
		}


		/* now write to the socket */
		notwritten = numsrc;							/* numsrc can be smaller than BUFF_SIZE */

        /* keep looping until nothing left to write*/
        while (notwritten > 0){
           /* notwritten = how much we have left to write
              sent_bytes_cnt  = how much we've written so far
              curr_sent = how much we've written in last write() call */
           curr_sent = write(sockfd, sendRecvBuff + sent_bytes_cnt, notwritten);
           if(curr_sent < 0){
        	   printf("ERROR writing to the socket: %s\n", strerror(errno));
        	   return errno;
           }

           sent_bytes_cnt  += curr_sent;
           notwritten -= curr_sent;
        }

        /* now read from the sockets the encrypted/decrypted bytes from the server */
        /* we need to read sent_bytes_cnt bytes */
        memset(sendRecvBuff, '0', sizeof(sendRecvBuff));

        /* filling the buffer with the server replay */
        while((recv_bytes_cnt < sent_bytes_cnt) && ((curr_read = read(sockfd, sendRecvBuff + recv_bytes_cnt, sent_bytes_cnt - recv_bytes_cnt)) > 0) ){
        	recv_bytes_cnt += curr_read;
        }

        if(recv_bytes_cnt < sent_bytes_cnt && curr_read < 0){
        	printf("ERROR reading from socket: %s\n", strerror(errno));
        	return errno;
        }

        if(recv_bytes_cnt != sent_bytes_cnt){
        	printf("ERROR, we should get back %d from the server, but it replies with %d bytes\n", sent_bytes_cnt, recv_bytes_cnt);
        	return -1;
        }

        written_bytes_cnt = 0;

        /* now write the encrypted/decrypted bytes to the output file */
        while (written_bytes_cnt < recv_bytes_cnt) {

        	curr_write = write(out_fd, sendRecvBuff + written_bytes_cnt, recv_bytes_cnt - written_bytes_cnt);

        	if(curr_write < 0){
				printf("ERROR write() to output file %s: %s\n", output_filename, strerror(errno));
				return errno;
			}

        	written_bytes_cnt += curr_write;
        }

        recv_bytes_cnt = 0;
        sent_bytes_cnt = 0;
    }

        close(in_fd);
        close(out_fd);
        close(sockfd);

        return 0;
    }
