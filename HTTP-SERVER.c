
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

void handle_get_request(int socket_fd);

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
    if ((status = getaddrinfo(NULL, "8080", &hints, &res) != 0)){
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

    printf("Server is listening on port 8080...\n");

    //Upon accepting a request we create a separate thread to manage the request
    while (1)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

        if (new_fd == -1){
            perror("accept");
            continue;
        }

        // Handle the HTTP request in a separate function
        handle_get_request(new_fd);
    }

    // Close the listening socket
    close(sockfd);
    return 0;
}

void handle_get_request(int socket_fd)
{
    // Buffer for the raw HTTP request
    char request[2048];
    // We'll parse the method, path, and version
    char method[8], path[512], http_version[16];

    // We serve files from the "www" directory
    const char *base_dir = "www";
    // Default file to serve if path == "/"
    const char *default_file = "index.html";

    // Receive the data
    int bytes_received = recv(socket_fd, request, sizeof(request) - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(socket_fd);
        return;
    }

    // Null-terminate the request string
    request[bytes_received] = '\0';

    // Debug (optional): print the raw request
    // printf("Raw request:\n%s\n", request);

    // Parse out the method, the path, and the HTTP version
    // Example request line: "GET /index.html HTTP/1.1"
    // We only handle the GET method in this example
    if (sscanf(request, "%7s %511s %15s", method, path, http_version) != 3) {
        // Malformed request
        const char *bad_request_response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "Bad Request";
        send(socket_fd, bad_request_response, strlen(bad_request_response), 0);
        close(socket_fd);
        return;
    }

    // We only care about GET for this minimal server
    if (strcmp(method, "GET") != 0) {
        const char *not_implemented_response =
            "HTTP/1.1 501 Not Implemented\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 15\r\n"
            "\r\n"
            "Not Implemented";
        send(socket_fd, not_implemented_response, strlen(not_implemented_response), 0);
        close(socket_fd);
        return;
    }

    // If the path is "/", serve the default file
    if (strcmp(path, "/") == 0) {
        snprintf(path, sizeof(path), "/%s", default_file);
    }

    // Build the full path to the requested file
    // Note: "path + 1" to skip the leading slash
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, path + 1);

    // Open the file
    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        // File not found
        const char *not_found_response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "404 Not Found";
        send(socket_fd, not_found_response, strlen(not_found_response), 0);
        close(socket_fd);
        return;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    // Read file into memory
    char *file_buf = malloc(file_size);
    if (!file_buf) {
        perror("malloc");
        fclose(fp);
        close(socket_fd);
        return;
    }
    
    fread(file_buf, 1, file_size, fp);
    fclose(fp);

    // Figure out a basic Content-Type (optional, minimal coverage)
    const char *content_type = "application/octet-stream";
    if (strstr(path, ".html") != NULL) {
        content_type = "text/html";
    } else if (strstr(path, ".css") != NULL) {
        content_type = "text/css";
    } else if (strstr(path, ".js") != NULL) {
        content_type = "application/javascript";
    } else if (strstr(path, ".png") != NULL) {
        content_type = "image/png";
    } else if (strstr(path, ".jpg") != NULL || strstr(path, ".jpeg") != NULL) {
        content_type = "image/jpeg";
    }

    // Build the HTTP response headers
    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "\r\n",
                              content_type, file_size);

    // Send headers
    send(socket_fd, header, header_len, 0);
    // Send file content
    send(socket_fd, file_buf, file_size, 0);

    // Clean up
    free(file_buf);
    close(socket_fd);
}

