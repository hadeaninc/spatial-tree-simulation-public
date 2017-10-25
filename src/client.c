#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#include "common.h"
struct ui_point *vertices = NULL;

#include "client-ui.c"
#include "client-tcp.c"

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s hostname port\n", argv[0]);
        exit(1);
    }
    vertices = malloc(sizeof(struct ui_point) * num_points);
    assert(vertices);
    pthread_t tcp_thread, ui_thread;
    pthread_create(&tcp_thread, NULL, tcp_init_and_loop, (void*)argv);
    pthread_create(&ui_thread, NULL, opengl_init_and_loop, NULL);
    pthread_join(ui_thread, NULL);
    exit(0);
}
