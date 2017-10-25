//glfw stuff
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include "linmath.h"

//various
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "morton.h"

static void error_callback(int error, const char* description) {
    fprintf(stderr, "glfw error: %s\n", description);
}

uint64_t num_workers = 0;

void process_packet(struct client_message *message, uint64_t id) {
    //do some point movement
    float time = glfwGetTime();
    if (id + 1 > num_workers) {
        num_workers = id + 1;
        vertices = realloc(vertices, sizeof(struct ui_point) * num_workers * num_points);
        assert(vertices);
        for (uint64_t i = 0; i < num_workers * num_points; i++) {
            float c = 0.5 + 0.5 * (float)rand() / RAND_MAX;
            vertices[i].c = (struct vector){0, 0.6 * c, 0.8 * c};
        }
    }
    if (!vertices)
        return;
    for (uint64_t i = 0; i < num_points; i++) {
        vertices[id * num_points + i].p = net_decode_position(message->points[i].net_encoded_position, message->worker_morton_id);
        //vertices[id * num_points + i].size = message->points[i].size;
        vertices[id * num_points + i].size = 6;
    }
}

void* opengl_init_and_loop(void *arg) {
    //initialise glfw and window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        exit(1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    //GLFWwindow *window = glfwCreateWindow(vidmode->width, vidmode->height, "demo", glfwGetPrimaryMonitor(), NULL);
    GLFWwindow *window = glfwCreateWindow(960, 720, "demo", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(1);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, 1);

    /*//create points array
    srand(time(NULL));
    for (uint64_t i = 0; i < num_points; i++) {
        vertices[i].p.x = -0.5 + ((float)rand() / RAND_MAX);
        vertices[i].p.y = -0.5 + ((float)rand() / RAND_MAX);
        vertices[i].p.z = -0.5 + ((float)rand() / RAND_MAX);
        float c = 0.5 + 0.5 * (float)rand() / RAND_MAX;
        vertices[i].c.r = 0.0 * c;
        vertices[i].c.g = 0.6 * c;
        vertices[i].c.b = 0.8 * c;
        vertices[i].size = 2 + 2 * ((float)rand() / RAND_MAX);
    }*/

    //create lines array
    struct line_vertex {
        float x, y, z;
    } line_vertices[4] = {
        {-0.5, -0.5, 0.5},
        { 0.5, -0.5, 0.5},
        { 0.5,  0.5, 0.5},
        {-0.5,  0.5, 0.5},
    };

    //setup the programs and pipeline for points
    #define QUOTE(...) #__VA_ARGS__
    static const char* point_vertex_shader_text =
        "#version 450\n" QUOTE(
        uniform mat4 mvp;
        in vec3 vcol;
        in vec3 vpos;
        in float vsize;
        out vec3 color;
        out gl_PerVertex {
            vec4 gl_Position;
            float gl_PointSize;
        };
        void main() {
            vec4 pos = mvp * vec4(vpos, 1.0);
            gl_Position = pos;
            gl_PointSize = vsize / pos.z;
            color = vcol;
        }
    );
    static const char* point_fragment_shader_text =
        "#version 450\n" QUOTE(
        in vec3 color;
        out vec4 gl_FragColor;
        void main() {
            if (distance(gl_PointCoord, vec2(0.5)) > 0.5)
                discard;
            else
                gl_FragColor = vec4(color * 0.8, 1.0);
        }
    );
    char error_log[4096] = {0};
    GLuint program_point_vertex = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &point_vertex_shader_text);
    glGetProgramInfoLog(program_point_vertex, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }
    GLuint program_point_fragment = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &point_fragment_shader_text);
    glGetProgramInfoLog(program_point_fragment, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }

    GLuint pipeline_point = 0;
    glGenProgramPipelines(1, &pipeline_point);
    glUseProgramStages(pipeline_point, GL_VERTEX_SHADER_BIT, program_point_vertex);
    glUseProgramStages(pipeline_point, GL_FRAGMENT_SHADER_BIT, program_point_fragment);

    //setup the programs and pipeline for lines
    static const char* line_vertex_shader_text =
        "#version 450\n" QUOTE(
        uniform mat4 mvp;
        in vec3 vpos;
        out vec3 color;
        out gl_PerVertex {
            vec4 gl_Position;
        };
        void main() {
            vec4 pos = mvp * vec4(vpos, 1.0);
            gl_Position = pos;
            color = vec3(0.5, 0.5, 0.5);
        }
    );
    static const char* line_fragment_shader_text =
        "#version 450\n" QUOTE(
        in vec3 color;
        out vec4 gl_FragColor;
        void main() {
            gl_FragColor = vec4(color, 1.0);
        }
    );
    GLuint program_line_vertex = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &line_vertex_shader_text);
    glGetProgramInfoLog(program_line_vertex, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }
    GLuint program_line_fragment = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &line_fragment_shader_text);
    glGetProgramInfoLog(program_line_fragment, sizeof(error_log), NULL, error_log);
    if (error_log[0] != 0) {
        puts(error_log);
        exit(1);
    }
    GLuint pipeline_line = 0;
    glGenProgramPipelines(1, &pipeline_line);
    glUseProgramStages(pipeline_line, GL_VERTEX_SHADER_BIT, program_line_vertex);
    glUseProgramStages(pipeline_line, GL_FRAGMENT_SHADER_BIT, program_line_fragment);

    //setup vao point
    GLuint vao_point;
    glGenVertexArrays(1, &vao_point);
    glBindVertexArray(vao_point);

    //setup point vertices
    GLuint buffer_point_vertices;
    glGenBuffers(1, &buffer_point_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_point_vertices);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
    GLint p_mvp_location = glGetUniformLocation(program_point_vertex, "mvp");
    GLint vpos_location = glGetAttribLocation(program_point_vertex, "vpos");
    GLint vcol_location = glGetAttribLocation(program_point_vertex, "vcol");
    GLint vsize_location = glGetAttribLocation(program_point_vertex, "vsize");
    glEnableVertexAttribArray(vpos_location);
    glEnableVertexAttribArray(vcol_location);
    glEnableVertexAttribArray(vsize_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(struct ui_point), (void*)offsetof(struct ui_point, p));
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(struct ui_point), (void*)offsetof(struct ui_point, c));
    glVertexAttribPointer(vsize_location, 1, GL_FLOAT, GL_FALSE, sizeof(struct ui_point), (void*)offsetof(struct ui_point, size));

    //setup vao line
    GLuint vao_line;
    glGenVertexArrays(1, &vao_line);
    glBindVertexArray(vao_line);

    //setup line vertices
    GLuint buffer_line_vertices;
    glGenBuffers(1, &buffer_line_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_line_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    GLint l_mvp_location = glGetUniformLocation(program_line_vertex, "mvp");
    GLint l_vpos_location = glGetAttribLocation(program_line_vertex, "vpos");
    glEnableVertexAttribArray(l_vpos_location);
    glVertexAttribPointer(l_vpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(struct line_vertex), (void*)offsetof(struct line_vertex, x));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    glfwSetTime(0);
    uint64_t frames = 0;
    struct vector camera_pos = {0, 0, 4};
    while (true) {
        struct timespec loop_start_time = timer_get();
        //handle user input
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) || glfwGetKey(window, GLFW_KEY_Q)) {
            glfwSetWindowShouldClose(window, true);
        }
        if (glfwGetKey(window, GLFW_KEY_W))
            camera_pos.z -= 0.1;
        if (glfwGetKey(window, GLFW_KEY_A))
            camera_pos.x -= 0.1;
        if (glfwGetKey(window, GLFW_KEY_S))
            camera_pos.z += 0.1;
        if (glfwGetKey(window, GLFW_KEY_D))
            camera_pos.x += 0.1;
        if (glfwGetKey(window, GLFW_KEY_SPACE))
            camera_pos.y += 0.1;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL))
            camera_pos.y -= 0.1;
        if (glfwWindowShouldClose(window)) {
            printf("%7.3ffps\n", frames / glfwGetTime());
            glfwDestroyWindow(window);
            glfwTerminate();
            exit(0);
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float) height;
        glViewport(0, 0, width, height);
        double mousex, mousey;
        glfwGetCursorPos(window, &mousex, &mousey);
        mousey = height - mousey;
        mousex = mousex / width - 0.5;
        mousey = mousey / height - 0.5;

        //setup mvp
        mat4x4 model, view, projection, mvp;
        mat4x4_translate(view, -camera_pos.x, -camera_pos.y, -camera_pos.z);
        mat4x4_perspective(projection, 120 * 2 * M_PI / 360, ratio, 0.1, 10);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(vao_point);
        for (uint64_t m = 0; m < num_workers; m++) {
            struct vector m_vec = morton_decode(m);
            uint32_t x = m_vec.x;
            uint32_t y = m_vec.y;
            mat4x4_translate(model, 0.5 + -4 + x, 0.5 + -4 + y, 0);
            mat4x4_mul(mvp, projection, view);
            mat4x4_mul(mvp, mvp, model);
            {
                //render points
                glBindVertexArray(vao_point);
                glBindBuffer(GL_ARRAY_BUFFER, buffer_point_vertices);
                //this vertices array is updated by the process packet function
                //glBufferSubData(GL_ARRAY_BUFFER, sizeof(struct ui_point) * num_points * m, sizeof(struct ui_point) * num_points, vertices + m * num_points);
                glBufferData(GL_ARRAY_BUFFER, sizeof(struct ui_point) * num_points * num_workers, vertices, GL_DYNAMIC_DRAW);
                glBindProgramPipeline(pipeline_point);
                glProgramUniformMatrix4fv(program_point_vertex, p_mvp_location, 1, GL_FALSE, (const GLfloat*)mvp);
                glDrawArrays(GL_POINTS, m * num_points, num_points);

                //render lines
                glClear(GL_DEPTH_BUFFER_BIT);
                glBindVertexArray(vao_line);
                glBindBuffer(GL_ARRAY_BUFFER, buffer_line_vertices);
                glLineWidth(4);
                glBindProgramPipeline(pipeline_line);
                glProgramUniformMatrix4fv(program_line_vertex, l_mvp_location, 1, GL_FALSE, (const GLfloat*)mvp);
                glDrawArrays(GL_LINE_LOOP, 0, sizeof(line_vertices) / sizeof(line_vertices[0]));
            }
        }

        glfwSwapBuffers(window);
        //for some reason glfwSwapBuffers is ignoring the swap interval, maybe a driver thing
        loop_start_time.tv_nsec += 1000000000ULL / target_fps;
        loop_start_time.tv_sec += loop_start_time.tv_nsec / 1e9;
        loop_start_time.tv_nsec %= 1000000000ULL;
        timer_sleep_until(loop_start_time);
        frames++;
    }
}
