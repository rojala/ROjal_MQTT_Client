
#include<stdio.h>      // printf
#include<sys/socket.h> // socket
#include<unistd.h>     // socket / file close
#include<arpa/inet.h>  // inet_addr
#include <pthread.h>   // pthread_create
#include <signal.h>    // pthread_kill

#include "socket_read_write.h"

static int test_socket = -1;
static socket_data_received_fptr_t socket_data_received_callback;
static pthread_t socket_reading_thread_id = -1;

static bool socket_OK = false;

void sigpipe_handler()
{
    printf("SIGPIPE caught\n");
    socket_OK = false;
}

int socket_write(uint8_t * a_data, size_t a_amount)
{
	return send(test_socket, a_data, a_amount , 0);
}
void *socket_receive_thread(void *socket_desc)
{
	char input_buffer[1024 * 100];
	int * sock = (int*)socket_desc;
	while ((*sock) > 0 && NULL != socket_data_received_callback) {
		int bytes_read = recv((*sock), input_buffer, sizeof(input_buffer), /*MSG_NOSIGNAL*/ 0);
		if (bytes_read > 0 && socket_data_received_callback) {
			socket_data_received_callback(input_buffer, bytes_read);
		} else {
			/* Exit from thread */
			//printf("WARNING: Socket read thread exit\n Soket:%i\n Bytes: %i\n Callback: %p\n\n", 
			//	   (*sock), bytes_read, socket_data_received_callback);
			//return 0;
		}
	}
	printf("Exit reading thread\n");
}

bool socket_initialize(char * a_inet_addr, uint32_t a_port, socket_data_received_fptr_t a_receive_callback)
{
    struct sockaddr_in server;
     
	// instal sigpipe handler
	signal(SIGPIPE, sigpipe_handler);

    //Create socket
    test_socket = socket(AF_INET , SOCK_STREAM , 0);
    if (test_socket == -1)
		return false;
    
	printf("Socket created %i\n", test_socket);
 
	int value = 1;
	setsockopt(test_socket,SOL_SOCKET,SO_REUSEADDR,&value, sizeof(int)); // https://stackoverflow.com/questions/10619952/how-to-completely-destroy-a-socket-connection-in-c

	socket_OK = true;
 
    server.sin_addr.s_addr = inet_addr(a_inet_addr);
    server.sin_family = AF_INET;
    server.sin_port = htons(a_port);
 
    //Connect to remote server
    if (connect(test_socket , (struct sockaddr *)&server , sizeof(server)) < 0)
		return false;
    
    printf("Socket connection successfully created\n");
	socket_data_received_callback = a_receive_callback;

	if( pthread_create( &socket_reading_thread_id, NULL, socket_receive_thread, (void*) &test_socket) < 0)
		return false;
	
	printf("Socket read thread successfully created\n");
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
				if( send(test_socket, &data, 0 , 0) < 0)
					socket_OK = false;
			}
			test_socket = -1;
			pthread_join(socket_reading_thread_id, NULL);
			socket_reading_thread_id = -1;
			return true;
	}
	return false;
}

