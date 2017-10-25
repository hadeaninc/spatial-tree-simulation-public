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
#include "worker.c"

int main(int argc, char *argv[]) {
    uint32_t width, height;
    if (argc != 5 || sscanf(argv[3], "%u", &width) != 1 || sscanf(argv[4], "%u", &height) != 1) {
        printf("usage: %s host port width height\n", argv[0]);
        return 1;
    }
    uint32_t num_workers = width * height;
    if (num_workers == 0) {
        printf("width * height must be > 0\n");
        return 1;
    }
    Process processes[num_workers];
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            struct worker_args worker_args = {
                .x = x,
                .y = y,
                .width = width,
                .height = height,
            };
            strlcpy(worker_args.host, argv[1], sizeof(worker_args.host));
            strlcpy(worker_args.port, argv[2], sizeof(worker_args.port));
            processes[y * width + x] = PROCESS_NEW(worker, &worker_args, sizeof(worker_args));
        }
    }


    uint32_t num_channels =
        num_workers * 4 - width * 2 - height * 2 +//4 directions per worker (minus the edges)
        num_workers * 2;//send to/recv from master per worker
    Channel channels[num_channels];
    uint32_t index = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            channels[index++] = (Channel){
                .src = SELF(),
                .dst = SIBLING(y * width + x),
            };
            channels[index++] = (Channel){
                .src = SIBLING(y * width + x),
                .dst = SELF(),
            };
        }
    }
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (x < width - 1)
                channels[index++] = (Channel){
                    .src = SIBLING(y * width + x),
                    .dst = SIBLING(y * width + (x+1)),
                };
        }
    }
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (y < height - 1)
                channels[index++] = (Channel){
                    .src = SIBLING(y * width + x),
                    .dst = SIBLING((y+1) * width + x),
                };
        }
    }
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (x > 0)
                channels[index++] = (Channel){
                    .src = SIBLING(y * width + x),
                    .dst = SIBLING(y * width + (x-1)),
                };
        }
    }
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            if (y > 0)
                channels[index++] = (Channel){
                    .src = SIBLING(y * width + x),
                    .dst = SIBLING((y-1) * width + x),
                };
        }
    }
    assert(index == num_channels);

    ProcessSet process_set = {
        .n_processes = num_workers,
        .n_channels = num_channels,
        .processes = processes,
        .channels = channels,
    };

    printf("spawning...\n");
    Spawn(&process_set);
    printf("spawning done\n");
    for (uint32_t i = 0; i < num_workers; i++) {
        PROCESS_DESTROY(processes[i]);
    }

    printf("opening channels...\n");
    Receiver *from_workers[num_workers];
    Sender *to_workers[num_workers];
    void *bufs[num_workers * 2];
    for (uint32_t i = 0; i < num_workers * 2; i++) {
        bufs[i] = malloc(channel_buf_size);
        assert(bufs[i]);
    }
    for (uint32_t i = 0; i < num_workers; i++) {
        to_workers[i] = OpenSender(bufs[i * 2 + 0], channel_buf_size);
        from_workers[i] = OpenReceiver(bufs[i * 2 + 1], channel_buf_size);
    }
    printf("opened channels\n");
    printf("initial send/recv...\n");
    for (uint32_t i = 0; i < num_workers; i++) {
        uint8_t start_message = 0;
        Send(to_workers[i], &start_message, sizeof(start_message));
    }
    for (uint32_t i = 0; i < num_workers; i++) {
        uint8_t start_message = 0;
        Receive(from_workers[i], &start_message, sizeof(start_message));
    }
    for (uint32_t i = 0; i < num_workers; i++) {
        uint8_t start_message = 0;
        Send(to_workers[i], &start_message, sizeof(start_message));
    }
    printf("initial send/recv done\n");
    printf("waiting for workers...\n");
    for (uint64_t tick = 0;; tick++) {
        struct master_message average = {0};
        for (uint32_t i = 0; i < num_workers; i++) {
            struct ping_message message;
            message.send_time[0] = timer_get();
            Send(to_workers[i], &message, sizeof(message));
        }
        for (uint32_t i = 0; i < num_workers; i++) {
            struct master_message report;
            Receive(from_workers[i], &report, sizeof(report));
            average.simulation_time += report.simulation_time / num_workers;
            average.loop_time += report.loop_time / num_workers;
        }
        //printf("tick %6lu\t%7.3fms\t%7.3fms\n", tick, average.simulation_time * 1000, average.loop_time * 1000);
    }
    for (uint32_t i = 0; i < num_workers; i++) {
        uint8_t stop_message = 0;
        Receive(from_workers[i], &stop_message, sizeof(stop_message));
    }
    printf("all workers completed\n");
    return 0;
}
