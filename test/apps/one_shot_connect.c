#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int setup_connect(char* addr, char* port, struct sockaddr* out_addr, socklen_t* out_len) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
        .ai_flags = AI_NUMERICSERV
    };

    struct addrinfo* results;
    int ret;

    ret = getaddrinfo(addr, port, &hints, &results);

    if(ret != 0) {
        printf("could not get addrinfo %s %s: %s\n", addr, port, gai_strerror(ret));
        return -1;
    }

    struct addrinfo* adrp;
    int sock = -1;
    for(adrp = results; adrp != NULL; adrp = adrp->ai_next) {
        sock = socket(adrp->ai_family, adrp->ai_socktype, adrp->ai_protocol);
        if(sock < 0) {
            continue;
        }
        break;
    }

    if(sock < 0) {
        printf("could not create socket: %s\n", strerror(errno));
        return -1;
    }

    if(adrp == NULL) {
        printf("could not find appropriate host %s %s\n", addr, port);
        return -1;
    }

    if(adrp->ai_addrlen > *out_len) {
        printf("given length is small\n");
        close(sock);
        return -1;
    }

    memcpy(out_addr, adrp->ai_addr, adrp->ai_addrlen);
    *out_len = adrp->ai_addrlen;

    freeaddrinfo(results);
    return sock;
}

int main(int argc, char* argv[]) {

    if(argc != 3) {
        printf("Usage: %s hostname port\n", argv[0]);
        exit(1);
    }

    struct sockaddr addr;
    socklen_t len = sizeof(addr);
    int sock_fd = setup_connect(argv[1], argv[2], &addr, &len);

    if(sock_fd == -1) {
        printf("could not setup connection\n");
        exit(1);
    }

    int err =
        connect(sock_fd, &addr, len);

    if(err) {
        printf("could not connect: %s\n", strerror(errno));
        exit(1);
    }

    int tmp;
    scanf("%d", &tmp); // blocking

    return 0;
}
