#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 720;

    constexpr float kMouseSensitivity = 0.1f;
    constexpr float kMoveSpeed = 5.0f;

    struct Camera
    {
        float posX = 0.0f;
        float posY = 0.0f;
        float posZ = 3.0f;

        float frontX = 0.0f;
        float frontY = 0.0f;
        float frontZ = -1.0f;

        float upX = 0.0f;
        float upY = 1.0f;
        float upZ = 0.0f;

        float yaw = -90.0f;
        float pitch = 0.0f;

        float lastX = static_cast<float>(kWindowWidth) * 0.5f;
        float lastY = static_cast<float>(kWindowHeight) * 0.5f;
        bool firstMouse = true;
    };

    Camera g_camera;

    float radians(const float degrees)
    {
        return degrees * static_cast<float>(M_PI) / 180.0f;
    }

    void updateCameraVectors(Camera& camera)
    {
        const float yawRad = radians(camera.yaw);
        const float pitchRad = radians(camera.pitch);

        camera.frontX = std::cos(yawRad) * std::cos(pitchRad);
        camera.frontY = std::sin(pitchRad);
        camera.frontZ = std::sin(yawRad) * std::cos(pitchRad);

        const float length = std::sqrt(
            camera.frontX * camera.frontX +
            camera.frontY * camera.frontY +
            camera.frontZ * camera.frontZ);

        if (length > 0.0f)
        {
            camera.frontX /= length;
            camera.frontY /= length;
            camera.frontZ /= length;
        }
    }

    void framebufferSizeCallback(GLFWwindow* window, const int width, const int height)
    {
        (void)window;
        glViewport(0, 0, width, height);
    }

    void mouseCallback(GLFWwindow* window, const double xpos, const double ypos)
    {
        Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
        if (camera == nullptr)
        {
            return;
        }

        const float x = static_cast<float>(xpos);
        const float y = static_cast<float>(ypos);

        if (camera->firstMouse)
        {
            camera->lastX = x;
            camera->lastY = y;
            camera->firstMouse = false;
        }

        const float xoffset = x - camera->lastX;
        const float yoffset = camera->lastY - y;
        camera->lastX = x;
        camera->lastY = y;

        camera->yaw += xoffset * kMouseSensitivity;
        camera->pitch += yoffset * kMouseSensitivity;

        if (camera->pitch > 89.0f)
        {
            camera->pitch = 89.0f;
        }
        if (camera->pitch < -89.0f)
        {
            camera->pitch = -89.0f;
        }

        updateCameraVectors(*camera);
    }

    void processInput(GLFWwindow* window, Camera& camera, const float deltaTime)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        const float velocity = kMoveSpeed * deltaTime;

        float rightX = camera.frontY * camera.upZ - camera.frontZ * camera.upY;
        float rightY = camera.frontZ * camera.upX - camera.frontX * camera.upZ;
        float rightZ = camera.frontX * camera.upY - camera.frontY * camera.upX;

        const float rightLength = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
        if (rightLength > 0.0f)
        {
            rightX /= rightLength;
            rightY /= rightLength;
            rightZ /= rightLength;
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            camera.posX += camera.frontX * velocity;
            camera.posY += camera.frontY * velocity;
            camera.posZ += camera.frontZ * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            camera.posX -= camera.frontX * velocity;
            camera.posY -= camera.frontY * velocity;
            camera.posZ -= camera.frontZ * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            camera.posX -= rightX * velocity;
            camera.posY -= rightY * velocity;
            camera.posZ -= rightZ * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            camera.posX += rightX * velocity;
            camera.posY += rightY * velocity;
            camera.posZ += rightZ * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            camera.posX += camera.upX * velocity;
            camera.posY += camera.upY * velocity;
            camera.posZ += camera.upZ * velocity;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
            camera.posX -= camera.upX * velocity;
            camera.posY -= camera.upY * velocity;
            camera.posZ -= camera.upZ * velocity;
        }
    }

    bool initGlfw()
    {
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW\n";
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        return true;
    }

    bool initGlad()
    {
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            std::cerr << "Failed to initialize GLAD\n";
            return false;
        }
        return true;
    }
}

int main()
{
    try
    {
        if (!initGlfw())
        {
            return EXIT_FAILURE;
        }

        GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Realtime SDF Ray Marcher", nullptr, nullptr);
        if (!window)
        {
            std::cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            return EXIT_FAILURE;
        }

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        glfwSetWindowUserPointer(window, &g_camera);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSwapInterval(1);

        if (!initGlad())
        {
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }

        glViewport(0, 0, kWindowWidth, kWindowHeight);
        updateCameraVectors(g_camera);

        const float quadVertices[] = {
            -1.0f,  1.0f,
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f
        };

        GLuint vao = 0;
        GLuint vbo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        Shader raymarchShader("shaders/raymarch.vert", "shaders/raymarch.frag");

        float timeSeconds = 0.0f;
        float lastFrameTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window))
        {
            const float currentFrameTime = static_cast<float>(glfwGetTime());
            const float deltaTime = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;
            timeSeconds += deltaTime;

            processInput(window, g_camera, deltaTime);

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            int framebufferWidth = kWindowWidth;
            int framebufferHeight = kWindowHeight;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

            raymarchShader.use();
            raymarchShader.setUniform("u_cameraPos", g_camera.posX, g_camera.posY, g_camera.posZ);
            raymarchShader.setUniform("u_cameraFront", g_camera.frontX, g_camera.frontY, g_camera.frontZ);
            raymarchShader.setUniform("u_cameraUp", g_camera.upX, g_camera.upY, g_camera.upZ);
            raymarchShader.setUniform(
                "u_resolution",
                static_cast<float>(framebufferWidth),
                static_cast<float>(framebufferHeight));
            raymarchShader.setUniform("u_time", timeSeconds);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);

        glfwDestroyWindow(window);
        glfwTerminate();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        glfwTerminate();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
