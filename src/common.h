#pragma once

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

const uint32_t target_fps = 30;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct vector {
    union {float x, r;};
    union {float y, g;};
    union {float z, b;};
};
struct sim_point {
    uint64_t id;
    struct vector p, v;
    float mass;
    float size;
};
struct net_point {
    uint32_t net_encoded_position;
    /*struct vector p;
    float size;*/
};
struct ui_point {
    struct vector p;
    struct vector c;
    float size;
};
struct tree_cell {
    uint64_t morton_code;
    uint64_t morton_mask;
};


struct worker_args {
    uint64_t x, y;
    uint64_t width, height;
    char host[16];
    char port[16];
};
struct __attribute__((packed)) ping_message {
    struct timespec send_time[4];
};
struct __attribute__((packed)) workers_message {
    uint64_t sender_morton;
    uint64_t sender_x, sender_y;
    struct timespec sender_time;
    uint8_t padding[4096 - 3 * sizeof(uint64_t) - sizeof(struct timespec)];
};
struct __attribute__((packed)) master_message {
    struct ping_message ping_message;
    float simulation_time;
    float loop_time;
    uint8_t padding[4096 - 2 * sizeof(float) - sizeof(struct ping_message)];
};
const uint64_t num_points = 300;
struct __attribute__((packed)) client_message {
    uint64_t worker_morton_id;
    struct net_point points[num_points];
};

#include "morton.h"
uint32_t net_encode_position(struct vector v, uint64_t morton) {
    struct vector worker_v = morton_decode(morton);
    v.x += 0.5;
    v.y += 0.5;
    v.z += 0.5;
    v.x *= (1 << 10);
    v.y *= (1 << 10);
    v.z *= (1 << 10);
    return
        (((uint32_t)v.x) <<  0) |
        (((uint32_t)v.y) << 10) |
        (((uint32_t)v.z) << 20);
}

struct vector net_decode_position(uint32_t p, uint64_t morton) {
    struct vector worker_v = morton_decode(morton);
    struct vector v = {0};
    v.x = (p >>  0) & ((1 << 10) - 1);
    v.y = (p >> 10) & ((1 << 10) - 1);
    v.z = (p >> 20) & ((1 << 10) - 1);
    v.x /= (1 << 10);
    v.y /= (1 << 10);
    v.z /= (1 << 10);
    v.x -= 0.5;
    v.y -= 0.5;
    v.z -= 0.5;
    return v;
}

float length(struct vector a) {
    return sqrt(pow(a.x, 2) + pow(a.y, 2) + pow(a.z, 2));
}
float dist(struct vector p0, struct vector p1) {
    return sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y) + (p1.z - p0.z) * (p1.z - p0.z));
};

struct timespec timer_get() {
    struct timespec time_struct;
    clock_gettime(CLOCK_REALTIME, &time_struct);
    return time_struct;
}
float timer_diff(struct timespec start_time, struct timespec stop_time){
    int64_t diff_nano = stop_time.tv_nsec - start_time.tv_nsec;
    int64_t diff = stop_time.tv_sec - start_time.tv_sec;
    return (float)diff + ((float)diff_nano / (float)1e9);
}
void timer_sleep_for(struct timespec time) {
    while (clock_nanosleep(CLOCK_REALTIME, 0, &time, &time));
}
void timer_sleep_until(struct timespec target_time) {
    struct timespec current_time = timer_get();
    while (current_time.tv_sec + current_time.tv_nsec / 1e9 < target_time.tv_sec + target_time.tv_nsec / 1e9) {
        current_time = timer_get();
    }
}
