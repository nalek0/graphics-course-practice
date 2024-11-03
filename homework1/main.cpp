#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>

#include "settings.hpp"
#include "shaders.hpp"

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct point {
    std::array<float, 2> position;
    std::array<float, 4> color;
};

std::array<point, (W + 1) * (H + 1)> makeTable(const float time) {
    std::array<point, (W + 1) * (H + 1)> points;
    
    for (int row = 0; row <= H; row++) {
        for (int column = 0; column <= W; column++) {
            std::array<float, 2> position = { -1.f + 2.f * column / W, -1.f + 2.f * row / H };
            float value = f(position[0], position[1], time);
            
            if (value > CHANGE_VALUE) {
                // Red color:
                float rv = std::min((value - CHANGE_VALUE) / (MAX_VALUE - CHANGE_VALUE), 1.f);
                std::array<float, 4> color = { 1.f, 1.f - rv, 1.f - rv, 1.f };
                points[row * (W + 1)+ column] = { position, color };
            } else {
                // Blue color:
                float bv = std::min((CHANGE_VALUE - value) / (CHANGE_VALUE - MIN_VALUE), 1.f);
                std::array<float, 4> color = { 1.f - bv, 1.f - bv, 1.f, 1.f };
                points[row * (W + 1) + column] = { position, color };
            }
        }
    }

    return points;
}
std::vector<std::uint32_t> makeIndices() {
    std::vector<std::uint32_t> result = {};

    for (int row = 0; row < H; row++) {
        for (int col = 0; col < W; col++) {
            std::uint32_t lti = row * (W + 1) + col;              // left top index
            std::uint32_t rti = (row + 1) * (W + 1) + col;        // right top index
            std::uint32_t lbi = row * (W + 1) + (col + 1);        // left bottom index
            std::uint32_t rbi = (row + 1) * (W + 1) + (col + 1);  // right bottom index

            // First triangle:
            result.push_back(lti);
            result.push_back(lbi);
            result.push_back(rti);
            // Second triangle:
            result.push_back(rti);
            result.push_back(lbi);
            result.push_back(rbi);
        }
    }

    return result;
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow("Graphics course homework 1",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        600, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(1.f, 1.f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    std::string project_root = PROJECT_ROOT;

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    // Init vao, vbo, ebo:
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0); // vec2 in_position
    glEnableVertexAttribArray(1); // vec4 in_color
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(point), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(point), (void *)(sizeof(std::array<float, 2>)));

    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    
    glBindVertexArray(0);

    // Event loop:
    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            button_down[event.key.keysym.sym] = false;
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;
        std::array<point, (W + 1) * (H + 1)> points = makeTable(time);
        std::vector<std::uint32_t> indices = makeIndices();
        // for (int i = 0; i < (W + 1) * (H + 1); i++) {
        //     std::cout << ((int) points[i].position[0] * 10) << " " << ((int) points[i].position[1] * 10) << "\n";
        // }
        // for (int i = 0; i < indices.size(); i++) {
        //     if (i % 3 == 0) {
        //         std::cout << std::endl;
        //     }
        //     std::cout << indices[i] << " ";
        // }
        // exit(1);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, (void *)(0));

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
