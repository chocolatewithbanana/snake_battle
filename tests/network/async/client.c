#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

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

        char format[] = " %00s";
        sprintf(format, " %%%ds", INET_ADDRSTRLEN-1);
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
        int ret = scanf("%hu", &port_h);

        if (ret != 1) {
            printf("Invalid port\n");
            continue;
        }

        break;
    }

    // Create client
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    pcr(fd, "Socket creation failed");

    pcr(fcntl(fd, F_SETFL, pcr(fcntl(fd, F_GETFL),
                               "fcntl get failed"
                              ) | O_NONBLOCK),
        "fcntl set O_NONBLOCK failed");

    struct sockaddr_in sock_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port_h),
        .sin_addr = addr
    };

    // Connect
    printf("Connecting\n");
    while (true) {
        int ret = connect(fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));

        if (ret < 0) {
            if (errno == EINPROGRESS) {
                continue;
            } else {
                errnoAbort("Connect failed");
            }
        }

        printf("Connected\n"); 
        break;
    }

    // Writing
    printf("Writing\n");
    time_t sec = 0;
    long ms = 500; 
    struct timespec time = {
        .tv_sec = sec,
        .tv_nsec = ms*1000000L
    };    
    struct timespec rem = {0};
    while (true) {
        while (nanosleep(&time, &rem) < 0) {
            time = rem;
        }

        int x = 2;
        ssize_t bytes = send(fd, &x, sizeof(x), MSG_NOSIGNAL);

        assert(bytes == sizeof(x) || bytes < 0);

        if (bytes < 0) {
            if (errno == ECONNRESET) {
                printf("Disconnected\n");
            } else {
                errnoAbort("Send failed");
            }
            break;
        }

        printf("Written\n");
    }

    return 0;
}
