//hadean includes
#include <hadean.h>
#include "hadean-defs.h"

//various includes
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>

//tcp includes
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "common.h"
#include "morton.h"
#include "simulate.c"

int worker(void * restrict arg_, uint64_t argLen_) {
    assert(argLen_ == sizeof(struct worker_args));
    struct worker_args const *args = arg_;
    assert(sizeof(struct workers_message) <= channel_buf_size);
    //four neighbours send/recv + one master send/recv
    void *bufs[10];
    for (uint32_t i = 0; i < 10; i++) {
        bufs[i] = malloc(channel_buf_size);
        assert(bufs[i]);
    }

    //setup master communication
    Receiver *from_master = OpenReceiver(bufs[8], channel_buf_size);
    Sender *to_master = OpenSender(bufs[9], channel_buf_size);

    //setup neighbour communication
    const uint64_t num_neighbours = 4;
    Sender *to_neighbours[num_neighbours] = {NULL};
    Receiver *from_neighbours[num_neighbours] = {NULL};
    //0 - right, 1 - top, 2 - left, 3 - bottom
    if (args->x < args->width - 1)  to_neighbours[0] = OpenSender(bufs[0], channel_buf_size);
    if (args->y < args->height - 1) to_neighbours[1] = OpenSender(bufs[1], channel_buf_size);
    if (args->x > 0)                to_neighbours[2] = OpenSender(bufs[2], channel_buf_size);
    if (args->y > 0)                to_neighbours[3] = OpenSender(bufs[3], channel_buf_size);
    if (args->x > 0)                from_neighbours[2] = OpenReceiver(bufs[6], channel_buf_size);
    if (args->y > 0)                from_neighbours[3] = OpenReceiver(bufs[7], channel_buf_size);
    if (args->x < args->width - 1)  from_neighbours[0] = OpenReceiver(bufs[4], channel_buf_size);
    if (args->y < args->height - 1) from_neighbours[1] = OpenReceiver(bufs[5], channel_buf_size);

    //initialise the entities
    struct sim_point *points = malloc(num_points * sizeof(struct sim_point));
    assert(points);
    initialise(points, num_points);

    //do initial send/recv
    uint8_t start_message = 0;
    Receive(from_master, &start_message, sizeof(start_message));
    Send(to_master, &start_message, sizeof(start_message));
    Receive(from_master, &start_message, sizeof(start_message));

    //setup tcp connection to client
    int sockfd;
    {
        struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *servinfo;
        int rv = getaddrinfo(args->host, args->port, &hints, &servinfo);
        if (rv != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }
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
            return 2;
        }
        char host_str[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), host_str, sizeof(host_str));
        printf("client: connecting to %s\n", host_str);
        int one = 1;
        setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
        freeaddrinfo(servinfo);
    }

    //main loop
    for (uint64_t tick = 0;; tick++) {
        struct timespec loop_start_time = timer_get();
        //for each neighbour direction, send then recv
        //to avoid blocking, the corresponding send/recv are called straight after the other
        if (to_neighbours[0]) {
            struct workers_message message = {
                .sender_x = args->x,
                .sender_y = args->y,
                .sender_time = timer_get(),
            };
            Send(to_neighbours[0], &message, sizeof(message));
        }
        if (from_neighbours[2]) {
            struct workers_message message = {0};
            Receive(from_neighbours[2], &message, sizeof(message));
            assert(message.sender_x == args->x - 1 && message.sender_y == args->y);
        }
        if (to_neighbours[1]) {
            struct workers_message message = {
                .sender_x = args->x,
                .sender_y = args->y,
                .sender_time = timer_get(),
            };
            Send(to_neighbours[1], &message, sizeof(message));
        }
        if (from_neighbours[3]) {
            struct workers_message message = {0};
            Receive(from_neighbours[3], &message, sizeof(message));
            assert(message.sender_x == args->x && message.sender_y == args->y - 1);
        }
        if (to_neighbours[2]) {
            struct workers_message message = {
                .sender_x = args->x,
                .sender_y = args->y,
                .sender_time = timer_get(),
            };
            Send(to_neighbours[2], &message, sizeof(message));
        }
        if (from_neighbours[0]) {
            struct workers_message message = {0};
            Receive(from_neighbours[0], &message, sizeof(message));
            assert(message.sender_x == args->x + 1 && message.sender_y == args->y);
        }
        if (to_neighbours[3]) {
            struct workers_message message = {
                .sender_x = args->x,
                .sender_y = args->y,
                .sender_time = timer_get(),
            };
            Send(to_neighbours[3], &message, sizeof(message));
        }
        if (from_neighbours[1]) {
            struct workers_message message = {0};
            Receive(from_neighbours[1], &message, sizeof(message));
            assert(message.sender_x == args->x && message.sender_y == args->y + 1);
        }


        //do the simulation
        struct timespec simulation_start_time = timer_get();
        simulate(points, num_points, 1.0f / target_fps);
        struct timespec simulation_end_time = timer_get();
        float simulation_time = timer_diff(simulation_start_time, simulation_end_time);


        //receive the ping from the master
        struct ping_message ping_message;
        Receive(from_master, &ping_message, sizeof(ping_message));
        ping_message.send_time[1] = timer_get();
        //send data to the client over tcp
        struct client_message message = {
            .worker_morton_id = morton_encode((struct vector){.x = args->x, .y = args->y}),
            /*.current_tick = tick,
            .ping_message = ping_message,*/
        };
        for (uint64_t i = 0; i < sizeof(message.points) / sizeof(message.points[0]); i++) {
            //message.points[i].p = points[i].p;
            //message.points[i].size = points[i].size;
            message.points[i].net_encoded_position = net_encode_position(points[i].p, message.worker_morton_id);
        }
        //snprintf((char*)&message.string[0], sizeof(message.string), "%lu", tick);
        send(sockfd, &message, sizeof(message), 0);


        struct timespec loop_end_time = timer_get();
        float loop_time = timer_diff(loop_start_time, loop_end_time);

        //send something back to the master
        struct master_message report = {.simulation_time = simulation_time, .loop_time = loop_time};
        Send(to_master, &report, sizeof(report));


        //sleep
        //next loop should start 33ms after this loop started
        //this either sleeps until the next absolute loop_start_time
        //or returns immediately if it is in the past
        //the while means if it's interrupted while sleeping, it tries again
        loop_start_time.tv_nsec += 1000000000ULL / target_fps;
        loop_start_time.tv_sec += loop_start_time.tv_nsec / 1e9;
        loop_start_time.tv_nsec %= 1000000000ULL;
        timer_sleep_until(loop_start_time);
    }

    //shutdown tcp connection
    close(sockfd);

    //final send
    uint8_t stop_message = 0;
    Send(to_master, &stop_message, sizeof(stop_message));
    printf("\t%lu completed\n", args->x);

    //shutdown channels
    CloseSender(to_master);
    CloseReceiver(from_master);
    if (to_neighbours[0])   CloseSender(to_neighbours[0]);
    if (to_neighbours[1])   CloseSender(to_neighbours[1]);
    if (to_neighbours[2])   CloseSender(to_neighbours[2]);
    if (to_neighbours[3])   CloseSender(to_neighbours[3]);
    if (from_neighbours[0]) CloseReceiver(from_neighbours[0]);
    if (from_neighbours[1]) CloseReceiver(from_neighbours[1]);
    if (from_neighbours[2]) CloseReceiver(from_neighbours[2]);
    if (from_neighbours[3]) CloseReceiver(from_neighbours[3]);

    return 0;
}
