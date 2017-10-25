#include <math.h>
#include <stdlib.h>
#include <time.h>

void initialise(struct sim_point *points, const uint64_t num_points) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    srand(t.tv_nsec);
    for (uint32_t i = 0; i < num_points; i++) {
        points[i] = (struct sim_point){
            .p = {
                -0.5 + ((float)rand() / RAND_MAX),
                -0.5 + ((float)rand() / RAND_MAX),
                -0.5 + ((float)rand() / RAND_MAX),
            },
            .v = {
                -0.5 + ((float)rand() / RAND_MAX),
                -0.5 + ((float)rand() / RAND_MAX),
                -0.5 + ((float)rand() / RAND_MAX),
            },
            .mass = 1 + 1 * ((float)rand() / RAND_MAX),
        };
        float density = 0.001;
        points[i].size = cbrt(3 * (points[i].mass / density) / (4 * M_PI));
    }
}


void simulate(struct sim_point *points, uint64_t num_points, float dt) {
    static struct vector *accelerations = NULL;
    if (!accelerations) {
        accelerations = calloc(num_points, sizeof(struct vector));
        assert(accelerations);
    }
    float mul = 1;

    enum {COULOMB_ATTRACTION, COULOMB_REPULSION, SWARM} mode = COULOMB_ATTRACTION;
    switch (mode) {
        default:
        case COULOMB_REPULSION:
            mul = -1;
        case COULOMB_ATTRACTION:
            //do some coulomb repulsion/attraction
            for (uint32_t i = 0; i < num_points; i++) {
                struct sim_point p0 = points[i];
                struct vector f = {0};
                for (uint32_t j = 0; j < num_points; j++) {
                    if (j == i)
                        continue;
                    struct sim_point p1 = points[j];
                    float r = dist(p0.p, p1.p);
                    if (r == 0)
                        continue;
                    float Fmag = mul * 0.1 * p0.mass * p1.mass / (r * r);
                    f.x += Fmag * (p1.p.x - p0.p.x);
                    f.y += Fmag * (p1.p.y - p0.p.y);
                    f.z += Fmag * (p1.p.z - p0.p.z);
                }
                accelerations[i].x = f.x / p0.mass;
                accelerations[i].y = f.y / p0.mass;
                accelerations[i].z = f.z / p0.mass;
            }
            break;
        case SWARM:
            //calculate accelerations from swarm behaviour rules
            for (uint32_t i = 0; i < num_points; i++) {
                struct sim_point p0 = points[i];

                //rule 1
                //move towards the average of the whole swarm
                struct vector r1 = {0};
                {
                    for (uint32_t j = 0; j < num_points; j++) {
                        if (j == i)
                            continue;
                        struct sim_point p1 = points[j];
                        r1.x += p1.p.x / (num_points-1);
                        r1.y += p1.p.y / (num_points-1);
                        r1.z += p1.p.z / (num_points-1);
                    }
                    r1.x *= 0.01;
                    r1.y *= 0.01;
                    r1.z *= 0.01;
                }

                //rule 2
                //move away from any close neighbours
                struct vector r2 = {0};
                {
                    for (uint32_t j = 0; j < num_points; j++) {
                        if (j == i)
                            continue;
                        struct sim_point p1 = points[j];
                        if (dist(p1.p, p0.p) < 0.15) {
                            r2.x -= (p1.p.x - p0.p.x);
                            r2.y -= (p1.p.y - p0.p.y);
                            r2.z -= (p1.p.z - p0.p.z);
                        }
                    }
                }

                //rule 3
                //align with the average direction of the whole swarm
                struct vector r3 = {0};
                {
                    for (uint32_t j = 0; j < num_points; j++) {
                        if (j == i)
                            continue;
                        struct sim_point p1 = points[j];
                        r3.x += p1.v.x / (num_points-1);
                        r3.y += p1.v.y / (num_points-1);
                        r3.z += p1.v.z / (num_points-1);
                    }
                    r3.x /= 8;
                    r3.y /= 8;
                    r3.z /= 8;
                }
                
                //sum the forces from the rules, convert to accelerations
                float mass = 1;
                accelerations[i] = (struct vector){
                    .x = (r1.x + r2.x + r3.x) / mass,
                    .y = (r1.y + r2.y + r3.y) / mass,
                    .z = (r1.z + r2.z + r3.z) / mass,
                };
            }
            break;
    }

    //apply the accelerations
    dt /= 10;
    for (uint32_t i = 0; i < num_points; i++) {
        struct sim_point p = points[i];
        struct vector a = accelerations[i];
        float friction = 0.995;
        p.v.x += a.x * dt;
        p.v.y += a.y * dt;
        p.v.z += a.z * dt;
        p.v.x *= friction;
        p.v.y *= friction;
        p.v.z *= friction;
        p.p.x += p.v.x * dt;
        p.p.y += p.v.y * dt;
        p.p.z += p.v.z * dt;
        if (p.p.x > 0.5) {p.p.x = 0.5; p.v.x *= -friction;}
        if (p.p.y > 0.5) {p.p.y = 0.5; p.v.y *= -friction;}
        if (p.p.z > 0.5) {p.p.z = 0.5; p.v.z *= -friction;}
        if (p.p.x < -0.5) {p.p.x = -0.5; p.v.x *= -friction;}
        if (p.p.y < -0.5) {p.p.y = -0.5; p.v.y *= -friction;}
        if (p.p.z < -0.5) {p.p.z = -0.5; p.v.z *= -friction;}
        points[i] = p;
    }
}
