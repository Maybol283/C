#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(void)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sockfd, new_fd, status;
    

    //Make sure hints is empty before setting
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //Call getaddrinfo and set to *res, 
    if ((status = getaddrinfo(NULL, 80, &hints, &res) != 0)){
        // If status != 0 then we have an error and it is printed
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        //Specific error return for getaddrinfo
        return 2;
    }

    //Create a socket file descriptor
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1){
        perror("socket");
        return 1;
    }

    //Bind the sockfd and supply the address and addrlength
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1){
        perror("bind");
        freeaddrinfo(res);
        return 1;
    }

    //Free memory after it has been bind    
    freeaddrinfo(res);
    
    //Listen on that sockfd and set a backlog to a max of 10
    if(listen(sockfd, 10) == -1){
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("Server is listening on port 80...\n");

    //Upon accepting a request we create a separate thread to manage the request
    while (1)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

        if (new_fd == -1){
            perror("accept");
            continue;
        }

    }

    close(sockfd);
    return 0;
}

void handle_get_request(int socket){
    //Set request to 1KB
    char request[1024];
    char *response; 
    //Receive the data using recv
    int bytes_received = recv(socket, request, sizeof(request) - 1, 0);
    
    //Error check
    if (bytes_received < 0) {
        perror("recv");
        close(socket);
        return;
    }

    //Ensure what has been received has be NULL terminated to avoid errors
    request[bytes_received] = '\0';
    
    //Check for GET request at top
    if (strncmp(request, "GET", 4) == 0){
        response = "Its a get request";
    } else {
        response = "Its not a get request";
    }

    send(socket, response, strlen(response), 0);

    close(socket);
}