#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

void errnoAbort(char* message) {
    perror(message);
    exit(1);
}

// posix check error
int pcr(int ret, char* message) {
    if (ret < 0) {
        errnoAbort(message);
    }
    return ret;
}

// posix check pointer
void* pcp(void* p, char* message) {
    if (!p) {
        errnoAbort(message);
    }
    return p;
}

int main() {
    // Get IP Address
    struct in_addr addr;
    while (true) {
        printf("Enter the IP address: "); 
        char s[INET_ADDRSTRLEN];

        //gets_sz(s, INET_ADDRSTRLEN);
        char format[] = " %00s";
        sprintf(format, " %%%ds", INET_ADDRSTRLEN);
        int scanf_ret = scanf(format, s);

        int pton_ret = pcr(
                  inet_pton(AF_INET, s, &addr),
                  "inet_pton failed");

        if (scanf_ret != 1 || pton_ret == 0) {
            printf("Invalid IP address\n");
            continue;
        }

        break;
    }

    // Get port
    uint16_t port_h;
    while (true) {
        printf("Enter the port: ");
        int ret_scanf = scanf("%hu", &port_h);

        if (ret_scanf != 1) {
            printf("Invalid input\n");
            continue;
        }

        break;
    }

    // Create server
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    pcr(fd, "Socket creation failed");

    pcr(
        fcntl(fd, F_SETFL, pcr(fcntl(fd, F_GETFL),
                               "fcntl get failed"
                              ) | O_NONBLOCK),
        "fcntl set O_NONBLOCK failed");

    struct sockaddr_in sock_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port_h),
        .sin_addr = addr
    };

    pcr(
        bind(fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)),
        "Bind failed");

    pcr(
        listen(fd, 1),
        "Listen failed");

    // Wait connections
    printf("Waiting connections\n");
    int client_fd;
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        client_fd = accept(fd, (struct sockaddr*)&client_addr, &len);

        if (client_fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue; 
        }

        pcr(client_fd, "Accept failed");

        pcr(
            fcntl(client_fd, F_SETFL, pcr(fcntl(client_fd, F_GETFL),
                                          "fcntl get failed"
                                         ) | O_NONBLOCK),
            "fcntl set O_NONBLOCK failed");

        char ip[INET_ADDRSTRLEN];
        pcp((void*)inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN),
            "Failed to get client ip");

        printf("Connected to ip=%s port=%d\n", ip, ntohs(client_addr.sin_port)); 
        break;
    }

    // Reading
    printf("Reading\n");
    time_t sec = 2;
    long ms = 0; 
    struct timespec time = {
        .tv_sec = sec,
        .tv_nsec = ms*1000000L
    };    
    struct timespec rem = {0};
    while (true) {
        while (nanosleep(&time, &rem) < 0) {
            time = rem;
        }

        int x;
        ssize_t bytes = recv(client_fd, &x, sizeof(x), MSG_PEEK);

        assert(bytes == sizeof(x) || bytes <= 0);

        if (bytes == 0) {
            printf("Disconnected\n");
            break;
        }

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Nothing\n");
                continue;
            } else {
                errnoAbort("Read failed");
            }
        }

        pcr(read(client_fd, &x, sizeof(x)), "Read failed");
        printf("Read\n");
    }

    return 0;
}
