/*** socket/demo2/client.c ***/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

int main(int argc, char* argv[]){
    // parse command line arguments
    int port_number = 25555;
    char* ip_address = "127.0.0.1";
    int opt;
    while ((opt = getopt(argc,argv, "i:p:h")) != -1) {
        switch (opt) {
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                printf("-i IP_address Default to \"127.0.0.1\";\n");
                printf("-p port_number Default to 25555;\n");
                printf("-h Display this help info.\n");
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "Error: Unknown option '-<%c>' received.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    int    server_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);

    /* STEP 1:
    Create a socket to talk to the server;
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(25555);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* STEP 2:
    Try to connect to the server.
    */
    connect(server_fd,
            (struct sockaddr *) &server_addr,
            addr_size);

    char buffer[1024];
    fd_set myset;
    FD_SET(server_fd, &myset);
    FD_SET(0,&myset);
    int maxfd = server_fd;

    while(1) {  
        // if you get a message from the server, print the question
        // if stdin, you have input to send and you scanf/fgets 
        /* Read a line from terminal  
           and send that line to the server
         */
        
        /* Receive response from the server */
        //FD_ZERO(&myset);
        FD_SET(server_fd,&myset);
        FD_SET(STDIN_FILENO,&myset);
        maxfd = server_fd;

        select(maxfd+1,&myset,NULL,NULL,NULL);

        if(FD_ISSET(server_fd,&myset)){

          int recvbytes = recv(server_fd, buffer, 1024, 0);
          if (recvbytes == 0) break;
          else {
            buffer[recvbytes] = 0;
            printf("%s", buffer); 
            fflush(stdout);
          }

        }
        
        if(FD_ISSET(0,&myset)) {
          scanf("%s", buffer);
          send(server_fd, buffer, 50, 0);
        }

    }

    close(server_fd);

  return 0;
}
