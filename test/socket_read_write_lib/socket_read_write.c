
#include <stdio.h>      // printf
#include <sys/socket.h> // socket
#include <unistd.h>     // socket / file close
#include <arpa/inet.h>  // inet_addr
#include <pthread.h>    // pthread_create
#include <signal.h>     // pthread_kill
#include <stdlib.h>     // malloc/free
#include <string.h>     // memcpy 
#include "socket_read_write.h"

static int test_socket = -1;
static socket_data_received_fptr_t socket_data_received_callback;
static pthread_t socket_reading_thread_id = -1;

static bool socket_OK = false;

void ctrlc_handler()
{
    socket_OK = false;
}

#define BUFFER_SIZE (1024*1024)

int socket_write(uint8_t * a_data, size_t a_amount)
{
	return send(test_socket, a_data, a_amount , 0);
}

int32_t get_remainingsize(uint8_t * a_input_ptr)
{
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t  cnt = 1;
    uint8_t  aByte = 0;

    /* Verify input parameters */
    if (NULL == a_input_ptr)
        return -1;

    do {
        aByte = a_input_ptr[cnt++];
        value += (aByte & 127) * multiplier;
        if (multiplier > (128*128*128))
            return -2;

        multiplier *= 128;
    } while ((aByte & 128) != 0);
	value += (cnt); //Add count header to the overal size of MQTT packet to be received
    return value;
}

void *socket_receive_thread(void *socket_desc)
{
	int * sock = (int*)socket_desc;
	while (((*sock) > 0) && 
		   (NULL != socket_data_received_callback)) {
        char header[32] = {0};
		int bytes_read = recv((*sock), header, sizeof(header) - 1, 0);
		
		if (2 <= bytes_read) {
			uint32_t remaining_bytes = get_remainingsize((uint8_t*)header) - bytes_read;
			
			if (0 < remaining_bytes) {
				/* Need to read more data */
				char * buff = (char*)malloc(remaining_bytes + bytes_read + 1024 /* safety buffer*/);
				memcpy(buff, header, bytes_read);
				while (remaining_bytes) {
					int nxt_bytes_read = recv((*sock), &buff[bytes_read], remaining_bytes, 0);
					remaining_bytes -= nxt_bytes_read;
					bytes_read += nxt_bytes_read;
					
				}
				socket_data_received_callback((uint8_t*)buff, (uint32_t)bytes_read);
				free(buff);
			} else {
				/* Send packets dirctly to the MQTT client, which fit into 32B buffer */
				socket_data_received_callback((uint8_t*)header, (uint32_t)bytes_read);
			}
		} else {
			char data = 0;
			if( send(test_socket, &data, 1 , 0) < 0)
				return 0;
		}
	}
	return 0;
}

bool socket_initialize(char * a_inet_addr, uint32_t a_port, socket_data_received_fptr_t a_receive_callback)
{
    struct sockaddr_in server;
     
	// catch ctrl c
	signal(SIGPIPE, ctrlc_handler);

    //Create socket
    test_socket = socket(AF_INET , SOCK_STREAM, 0);
    if (test_socket == -1)
		return false;
     
	int value = 1;
	setsockopt(test_socket,SOL_SOCKET,SO_REUSEADDR,&value, sizeof(int)); // https://stackoverflow.com/questions/10619952/how-to-completely-destroy-a-socket-connection-in-c

	socket_OK = true;
 
    server.sin_addr.s_addr = inet_addr(a_inet_addr);
    server.sin_family = AF_INET;
    server.sin_port = htons(a_port);
 
    //Connect to remote server
    if (connect(test_socket , (struct sockaddr *)&server , sizeof(server)) < 0)
		return false;
    
	socket_data_received_callback = a_receive_callback;

	if( pthread_create( &socket_reading_thread_id, NULL, socket_receive_thread, (void*) &test_socket) < 0)
		return false;
	
	return true;
}

bool stop_reading_thread()
{
	if (0 < test_socket) {
			//close(test_socket);
			shutdown(test_socket, 2 /* Ignore and stop both RCV and SEND */); //http://www.gnu.org/software/libc/manual/html_node/Closing-a-Socket.html
			while(socket_OK) {
				struct timespec ts;
				ts.tv_sec = 100 / 1000;
				ts.tv_nsec = (100 % 1000) * 1000000;
				nanosleep(&ts, NULL);
				char data = 0;
				if( send(test_socket, &data, 1 , 0) < 0)
					socket_OK = false;
			}
			test_socket = -1;
			pthread_join(socket_reading_thread_id, NULL);
			socket_reading_thread_id = -1;
			return true;
	}
	return false;
}

