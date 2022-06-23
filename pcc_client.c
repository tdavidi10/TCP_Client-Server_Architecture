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
#include <fcntl.h>


void send_N(int socket_fd, void *buffer)
{
    int total_sent, nsent, not_written;
    not_written = sizeof(uint64_t);
    total_sent = 0;
    
    while(not_written > 0)
    {
        /* write to socket from buffer[total_sent] at most not_written bytes */
        nsent = write(socket_fd, buffer + total_sent, not_written); 
        if (nsent < 0) {
            perror("Error: failed sending N: ");
            exit(1);
        }
        total_sent  += nsent;
        not_written -= nsent;
    }
}


int main(int argc, char *argv[]){
    char* sever_ip_str; /* argv[1] - server's IP address in string */
    uint16_t server_port; /* argv[2] - server's port number - 16-bit unsigned integer*/
    char* file_path; /* argv[3] - path of the file to send */
    FILE* file; /* file descriptor of the file to send */
    int socket_fd;
    struct sockaddr_in serv_addr; /* server's address - saw in TIRGUL*/
    uint64_t N; /* number of bytes - file_size */
    uint64_t N_network; /* number of bytes - file_size - network byte order */
    uint64_t C; /* number of printable characters */
    
    int total_sent; /* total number of bytes sent so far */
    int total_read; /* total number of bytes read so far */
    int not_ridden;
    int not_written; /* number of bytes not written yet */
    int nsent; /* number of bytes sent in the last iteration */
    int nread; /* number of bytes read in the last iteration */
    char file_buffer[100000]; /* buffer for file 100KB < 1MB as required */
    char* readc_buffer; /* buffer for C */
    int bytes_readc; /* number of bytes read in the last iteration in C*/


    if (argc != 4) { /* argc has to be 4 (3 real arguments) */
        printf("Error: wrong number of arguments \n");
        exit(1);
    }
    sever_ip_str = argv[1];
    server_port = atoi(argv[2]); 
    file_path = argv[3];
    /* open the file */
    file = fopen(file_path, "r"); /* open the file in read-only mode */
    if (file == NULL) {
        perror("Error: failed openning file: ");
        exit(1);
    }
    /* create a socket */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0); /* create a TCP socket: AF_INET(IPV4), SOCK_STREAM(TCP) */
    if (socket_fd < 0) {
        perror("Error: failed creating socket: ");
        exit(1);
    }
    /* set the server's address */
    memset(&serv_addr, 0, sizeof(serv_addr)); /* first clean the struct */
    serv_addr.sin_family = AF_INET; /* IPV4 */
    serv_addr.sin_port = htons(server_port); /* 16bit server's port number + adjusting endiannes */
    if (inet_pton(AF_INET, sever_ip_str, &(serv_addr.sin_addr.s_addr)) != 1){ /* server's IP address */
        perror("Error: failed converting IP address: ");
        exit(1);
    }
    /* connect to the server */
    if (connect(socket_fd,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        perror("Error: failed connecting to server: ");
        exit(1);
    }
    /* send file to the server according to protocol */
    /* a. send N - file_size in 64-bit unsigned integer in network byte order */
    
    /* get the file size, origin: https://www.geeksforgeeks.org/c-program-find-size-file/ */
    fseek(file, 0L, SEEK_END);
    N = ftell(file); /* N is length of file in bytes */
    N_network = htobe64(N); /* convert to big endian==network byte order */
    fseek(file, 0L, SEEK_SET); /* set the file to the start - "reset file" */

    /* send N in network byte order as seen in TIRGUL */    
    send_N(socket_fd, &N_network);
    /* b. send the server N bytes (the file’s content) */
    /* read the file and send it to the server */

    total_sent = 0;
    not_written = N; /* we have to send N bytes */
    /* keep looping until nothing left to write */
    while(not_written > 0)
    {
        /* read from file[total_sent] to file_buffer at most 1 byte */
        nread = fread(file_buffer, 1, 100000,file); /* reads from file to file_buffer 100000 elements of 1 byte */
        if (nread < 0) {
            perror("Error: failed reading file: ");
            exit(1);
        }

        /* write to socket from file_buffer at most not_written bytes */
        nsent = write(socket_fd, file_buffer, nread); 
        if (nsent < 0) {
            perror("Error: failed sending N bytes: ");
            exit(1);
        }
        total_sent  += nsent;
        not_written -= nsent;
        /* reset the file_buffer */
        memset(file_buffer, 0, sizeof(file_buffer)); 
    }

    /* close file, no need of file anymore */
    fclose(file);


    /* as seen in TIRGUL */
    /* c. read C- number of printable characters- 64-bit unsigned integer in network byte order from server */
    /* allocate memory for readc_buffer */
    readc_buffer = (char*)malloc(sizeof(uint64_t));
    if (readc_buffer == NULL) {
        perror("Error: failed allocating memory for readc_buffer: ");
        exit(1);
    }
     /* reset the buffer */
    memset(readc_buffer, 0, sizeof(uint64_t)); /* reset the buffer */
    total_read = 0;
    not_ridden = 8; /* we have to read 8 bytes */
    while(not_ridden > 0) /* while is there anything to read */
    {
        bytes_readc = read(socket_fd, readc_buffer + total_read, not_ridden);  
        if(bytes_readc < 0)
        {
            perror("Error: failed reading C: ");
            exit(1);
        }
        total_read += bytes_readc;
        not_ridden -= bytes_readc;
    } 
    /* convert to host byte order */
    C = be64toh(*(uint64_t*)readc_buffer);
    /* print the number of printable characters */
    printf("# of printable characters: %lu\n", C);
    close(socket_fd);
    exit(0);

}






































