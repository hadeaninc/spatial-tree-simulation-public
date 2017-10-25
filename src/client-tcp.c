//various includes
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>

//tcp includes
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "common.h"

void* tcp_init_and_loop(void *arg) {
    char **argv = (char**)arg;
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *servinfo;
    int rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    int sockfd;
    struct addrinfo *p;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        break;
    }
    if (!p) {
        fprintf(stderr, "client: failed to connect\n");
        exit(1);
    }
    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo);

    uint64_t num_workers = 1024;
    struct client_message *messages = calloc(num_workers, sizeof(struct client_message));
    assert(messages);
    int *recvbytes_msg = calloc(num_workers, sizeof(int));
    assert(recvbytes_msg);
    while (1) {
        //read in a whole multiplexer_header
        struct __attribute__((packed)) multiplexer_header {
            uint64_t id;
            uint64_t len;
        } header = {0};
        for (int recvbytes = 0; recvbytes < sizeof(header);) {
            int numbytes = recv(sockfd, ((uint8_t*)&header)+recvbytes, sizeof(header)-recvbytes, 0);
            if (numbytes == -1) {
                perror("recv");
                exit(1);
            }
            recvbytes += numbytes;
        }
        if (header.id >= num_workers) {
            while (header.id >= num_workers)
                num_workers *= 2;
            recvbytes_msg = realloc(recvbytes_msg, sizeof(int) * num_workers);
            assert(recvbytes_msg);
            messages = realloc(messages, sizeof(struct client_message) * num_workers);
            assert(messages);
        }
        //read in whatever data from the multiplexer
        for (int recvbytes = 0; recvbytes < header.len;) {
            //is the end of the message closer? or the end of the multiplexer packet
            uint64_t rem0 = header.len - recvbytes;
            uint64_t rem1 = sizeof(messages[0]) - recvbytes_msg[header.id];
            uint64_t rem = rem0 < rem1 ? rem0 : rem1;
            assert(rem > 0);
            int numbytes = recv(sockfd, (uint8_t*)&messages[header.id]+recvbytes_msg[header.id], rem, 0);
            if (numbytes == -1) {
                perror("recv");
                exit(1);
            }
            recvbytes += numbytes;
            recvbytes_msg[header.id] += numbytes;
            if (recvbytes_msg[header.id] == sizeof(messages[0])) {
                //complete packet, process it
                recvbytes_msg[header.id] = 0;
                process_packet(&messages[header.id], header.id);
            }
        }
    }
    close(sockfd);
    exit(0);
}
