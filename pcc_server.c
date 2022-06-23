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
#include <endian.h>
#include <signal.h>


int called_sigint_when_busy;
int is_processing_client; /* if we are processing client's request */
uint64_t pcc_total[95]; /* array of 95 count for each printable char seen and his count offset -32*/


void my_sigint_handler(){
    if(is_processing_client == 0) { /* if not processing client's request */
        for(int i = 0; i < 95; i++) { /* print pcc_total */
            printf("char '%c' : %lu times\n", (i+32), pcc_total[i]);
        }
        exit(0);
    }
    else{ /* if processing client's request now */
        called_sigint_when_busy = 1; /* remember to not take another request after this one. after this one print stats */
    }
}

/* we update the SIGINT handler to: wait for proccesing client to end, then print statisticts, then exit */
void update_sigint_handler() 
{   
    /* https://www.youtube.com/watch?v=jF-1eFhyz1U&t=220s here learned how to create the handler */
    int state_sigint;
    state_sigint = 0;
    struct sigaction sigi;
    sigi.sa_handler = my_sigint_handler; /* The default action is to terminate the process */
    sigi.sa_flags = SA_RESTART;  /* avoiding the EINTR, SA_RESTART lets handler work acoording to documentation in https://www.gnu.org/software/libc/manual/html_node/Flags-for-Sigaction.html and the video in up link */
    state_sigint = sigaction(SIGINT, &sigi, NULL);
    if (state_sigint != 0) /* if handle fails */
    {
        perror("Error: failed to update SIGINT handler: ");
        exit(1);
    }
    return;
}


int send_C(int socket_fd, void* buffer) {
    int total_written, nsent, not_written;
    total_written = 0;
    not_written = sizeof(uint64_t);
    while(not_written > 0){
        nsent = write(socket_fd, buffer + total_written, not_written); /* write the printable_char_count to the client */
        if(nsent < 0){
            perror("Error: failed writing to client: ");
            if((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE) || (errno == EOF))
            {
                return 1;
            } 
            else
            {
                exit(1);
            }
        }
        not_written -= nsent;
        total_written += nsent;
    }
    return 0;
}
    

int main(int argc, char *argv[]){

    uint16_t server_port; /* argv[1] - server's port number - 16-bit unsigned integer*/
    int listen_socket; /* socket for listening */
    struct sockaddr_in serv_addr; /* server's address - saw in TIRGUL*/
    int socket_fd; /* socket after accepting connection */
    struct sockaddr_in peer_addr; /* client's address - saw in TIRGUL*/
    char read_buffer[100000]; /* buffer for reading 100KB char each time to add it (or not) to pcc_total */
    int bytes_read; 
    char* readn_buffer; /* buffer for reading N */
    int total_read;
    int not_ridden;
    uint64_t N;
    uint64_t printable_char_count; /* how many printable chars seen in this connection */
    int client_error;
    uint64_t pcc_client[95];
    uint64_t c_network; /* network byte order */

    is_processing_client = 0;
    called_sigint_when_busy = 0;
    for(int i = 0; i < 95; i++) { /* reset pcc_total */
        pcc_total[i] = 0;
    }

    update_sigint_handler(); 
    
    socklen_t addrsize; /* size of the address */
    addrsize = sizeof(struct sockaddr_in);
    client_error = 0;
    
    if (argc != 2) { /* argc has to be 2 (1 real argument) */
        printf("Error: wrong number of arguments\n");  
        exit(1);
    }
    
    server_port = atoi(argv[1]);
    listen_socket = socket(AF_INET, SOCK_STREAM, 0); /* create a TCP listening socket: AF_INET(IPV4), SOCK_STREAM(TCP) */
    if (listen_socket < 0) {
        perror("Error: failed creating listening socket: ");
        exit(1);
    }
    /* https://www.codegrepper.com/code-examples/whatever/example+SO_REUSEADDR ---- here figured out how to set SO_REUSEADDR */
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("Error: failed to set socket option SO_REUSEADDR: ");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* INADDR_ANY = any local machine address */
    serv_addr.sin_port = htons(server_port); /* convert port number from host byte order to network byte order */
    if (bind(listen_socket, (struct sockaddr*) &serv_addr, addrsize) < 0) { /* bind the socket to the server's address */
        perror("Error: failed binding listening socket: ");
        exit(1);
    }

    /* Listen to incoming TCP connections on the erver port */
    if (listen(listen_socket, 10) < 0) { /* queue of size 10 connections */
        perror("Error: failed listening on listening socket: ");
        exit(1);
    }

    /* Enter a loop, in which each iteration: */
    while(1){
        for(int i = 0; i < 95; i++) { /* reset pcc_client */
            pcc_client[i] = 0;
        }
        client_error = 0;
        if(called_sigint_when_busy == 1) { /* if not processing client's request */
            for(int i = 0; i < 95; i++) { /* print pcc_total */
                printf("char '%c' : %lu times\n", (i+32), pcc_total[i]);
            }
            exit(0);
        }

        is_processing_client = 0; /* we are not processing client's request yet */
        printable_char_count = 0; /* reset printable_char_count for this connection */
        socket_fd = accept(listen_socket, (struct sockaddr*) &peer_addr, &addrsize); /* accept a connection */
        if (socket_fd < 0) {
            perror("Error: failed accepting connection: ");
            exit(1);
        }

        is_processing_client = 1; /* we are processing client's request now */
        /* read N from client */
        readn_buffer = (char*)malloc(sizeof(uint64_t));
        if(readn_buffer == NULL){
            perror("Error: failed allocating memory for readn_buffer: ");
            exit(1);
        }
        memset(readn_buffer, 0, sizeof(uint64_t)); /* reset the buffer */
        total_read = 0;
        not_ridden = 8; /* 8 bytes to read - N is 8 bytes */
        while(not_ridden > 0){ /* while there are still bytes to read */
            bytes_read = read(socket_fd, readn_buffer + total_read, not_ridden);
            if(bytes_read < 0){
                perror("Error: failed reading N from client: ");
                if((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE) || (errno == EOF))
                {
                    client_error = 1;
                } 
                else
                {
                    exit(1);
                }
            }
            total_read += bytes_read;
            not_ridden -= bytes_read;
        }
        N = be64toh(*(uint64_t*)readn_buffer); /* N is 8 bytes and adjust endians */
        /* read N bytes from client - read the file */
        total_read = 0;
        not_ridden = N; /* N bytes to read */
        while(not_ridden > 0){
            bytes_read = read(socket_fd, read_buffer, 100000); /* read 100000 char */        
            if(bytes_read < 0){
                perror("Error: failed reading from client: ");
                if((errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE) || (errno == EOF))
                {
                    client_error = 1;
                } 
                else
                {
                    exit(1);
                }
            }

           /* update pcc_client */
            for(int i = 0; i < bytes_read; i++){ /* for each char in the read_buffer */
                if((32 <= read_buffer[i]) && (read_buffer[i] <= 126)){ /* if the char is printable */
                    pcc_client[(int)(read_buffer[i]-32)]++; /* add it to pcc_client */
                    printable_char_count++; /* add to this connection printable char count */
                }
            }
            not_ridden = not_ridden - bytes_read; /* update not_ridden */
            total_read = total_read + bytes_read; /* update total_read */
        }
        /* write the printable_char_count to the client */
        c_network = htobe64(printable_char_count);

        if (send_C(socket_fd, &c_network) == 1) /* send C */
            client_error = 1;
        
        /* update pcc_total only if no error in client */
        if(client_error == 0){
            for(int i = 0; i < 95; i++) {
                pcc_total[i] += pcc_client[i];
            }
        }
        
    }

































}
































