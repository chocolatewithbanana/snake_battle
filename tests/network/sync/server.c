#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

// posix check error (< 0)
int pcr(int ret, char* message) {
    if (ret < 0) {
        perror(message);
        exit(1);
    }
    return ret;
}

// posix check error (<= 0)
int pcr0(int ret, char* message) {
    if (ret <= 0) {
        perror(message);
        exit(1);
    }
    return ret;
}

// posix check pointer


int main() {
    // Get IP Address
    printf("Enter the IP address: "); 
    char s[51];
    scanf(" %50s", s);

    struct in_addr addr;
    pcr0(inet_pton(AF_INET, s, &addr),
            "inet_pton failed");

    // Get port
    printf("Enter the port: ");
    uint16_t port_h;
    scanf("%hu", &port_h);

    // Create server
    printf("[INFO] Creating server\n");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    pcr(fd, "Socket creation failed");

    /*
    pcr(fcntl(fd, F_SETFL, pcr(fcntl(fd, F_GETFL),
                               "fcntl get failed"
                           ) | O_NONBLOCK),
        "fcntl set O_NONBLOCK failed");
        */

    struct sockaddr_in sock_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port_h),
        .sin_addr = addr
    };

    pcr(bind(fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)),
        "Bind failed");

    pcr(listen(fd, 1),
            "Listen failed");

    // Wait connections
    printf("[INFO] Waiting connections\n");
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(fd, (struct sockaddr*)&client_addr, &len);
        pcr(client_fd, "Not possible to connect");

        printf("Connected\n"); 
    }

    return 0;
}
