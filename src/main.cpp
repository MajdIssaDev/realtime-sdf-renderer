#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Shader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 720;

    constexpr float PI = 3.14159265358979323846f;
    constexpr float kMouseSensitivity = 0.1f;
    constexpr float kMoveSpeed = 5.0f;
    constexpr float kFocalLength = 1.5f;
    constexpr float kHitEps = 0.0005f;

    constexpr int kPickNone = 0;
    constexpr int kPickSphere = 1;
    constexpr int kPickBox = 2;

    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct ObjectMaterial
    {
        float color[3];
        float roughness = 0.2f;
        float specular = 0.85f;
        float metalness = 0.8f;
    };

    struct MaterialState
    {
        ObjectMaterial sphere{ { 1.0f, 0.84f, 0.0f }, 0.2f, 0.85f, 0.8f };
        ObjectMaterial box{ { 0.1f, 0.3f, 0.8f }, 0.9f, 0.0f, 0.0f };
    };

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
    MaterialState g_materials;
    bool g_isTrackingCamera = false;
    bool g_timePaused = false;
    int g_selectedObjectID = kPickNone;
    float g_timeSeconds = 0.0f;

    bool g_pickRequested = false;
    float g_pickMouseX = 0.0f;
    float g_pickMouseY = 0.0f;

    float radians(const float degrees)
    {
        return degrees * PI / 180.0f;
    }

    Vec3 normalizeVec3(const Vec3 v)
    {
        const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length <= 0.0f)
        {
            return Vec3{};
        }

        return Vec3{ v.x / length, v.y / length, v.z / length };
    }

    Vec3 crossVec3(const Vec3 a, const Vec3 b)
    {
        return Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    float lengthVec3(const Vec3 a, const Vec3 b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    void updateCameraVectors(Camera& camera)
    {
        const float yawRad = radians(camera.yaw);
        const float pitchRad = radians(camera.pitch);

        camera.frontX = std::cos(yawRad) * std::cos(pitchRad);
        camera.frontY = std::sin(pitchRad);
        camera.frontZ = std::sin(yawRad) * std::cos(pitchRad);

        const Vec3 front = normalizeVec3(Vec3{ camera.frontX, camera.frontY, camera.frontZ });
        camera.frontX = front.x;
        camera.frontY = front.y;
        camera.frontZ = front.z;
    }

    float sdBox(const Vec3 p, const Vec3 halfExtents)
    {
        const float qx = std::fabs(p.x) - halfExtents.x;
        const float qy = std::fabs(p.y) - halfExtents.y;
        const float qz = std::fabs(p.z) - halfExtents.z;

        const float outside = std::sqrt(
            std::max(qx, 0.0f) * std::max(qx, 0.0f) +
            std::max(qy, 0.0f) * std::max(qy, 0.0f) +
            std::max(qz, 0.0f) * std::max(qz, 0.0f));
        const float inside = std::min(std::max(qx, std::max(qy, qz)), 0.0f);

        return outside + inside;
    }

    float sdPlane(const Vec3 p, const float height)
    {
        return p.y - height;
    }

    Vec3 sphereCenterAtTime(const float timeSeconds)
    {
        return Vec3{
            std::sin(timeSeconds) * 2.0f,
            0.5f + 0.3f * std::cos(timeSeconds * 1.5f),
            std::cos(timeSeconds) * 2.0f
        };
    }

    float boxDistOnly(const Vec3 p, const float timeSeconds)
    {
        (void)timeSeconds;
        return sdBox(p, Vec3{ 1.0f, 1.0f, 1.0f });
    }

    float sphereDistOnly(const Vec3 p, const float timeSeconds)
    {
        return lengthVec3(p, sphereCenterAtTime(timeSeconds)) - 1.0f;
    }

    float floorDistOnly(const Vec3 p, const float timeSeconds)
    {
        (void)timeSeconds;
        return sdPlane(p, -1.0f);
    }

    using DistFn = float (*)(const Vec3, const float);

    bool marchToSurface(const Vec3 ro, const Vec3 rd, const DistFn distFn, const float timeSeconds, float& outT)
    {
        float t = 0.0f;

        for (int i = 0; i < 100; ++i)
        {
            const Vec3 p{
                ro.x + rd.x * t,
                ro.y + rd.y * t,
                ro.z + rd.z * t
            };

            const float d = distFn(p, timeSeconds);
            if (d < kHitEps)
            {
                outT = std::max(t - kHitEps, 0.0f);
                return true;
            }

            t += d;
            if (t > 100.0f)
            {
                return false;
            }
        }

        return false;
    }

    Vec3 buildCameraRay(const Camera& camera, const float mouseX, const float mouseY, const int framebufferWidth, const int framebufferHeight)
    {
        const Vec3 forward = normalizeVec3(Vec3{ camera.frontX, camera.frontY, camera.frontZ });
        const Vec3 worldUp{ camera.upX, camera.upY, camera.upZ };
        const Vec3 right = normalizeVec3(crossVec3(forward, worldUp));
        const Vec3 up = normalizeVec3(crossVec3(right, forward));

        const float pixelX = mouseX;
        const float pixelY = static_cast<float>(framebufferHeight) - mouseY;

        float ndcX = (pixelX / static_cast<float>(framebufferWidth)) * 2.0f - 1.0f;
        float ndcY = (pixelY / static_cast<float>(framebufferHeight)) * 2.0f - 1.0f;
        ndcX *= static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

        const Vec3 rd{
            ndcX * right.x + ndcY * up.x + kFocalLength * forward.x,
            ndcX * right.y + ndcY * up.y + kFocalLength * forward.y,
            ndcX * right.z + ndcY * up.z + kFocalLength * forward.z
        };

        return normalizeVec3(rd);
    }

    int pickObjectAlongRay(const Camera& camera, const Vec3 rayDir, const float timeSeconds)
    {
        const Vec3 ro{ camera.posX, camera.posY, camera.posZ };

        float tBox = -1.0f;
        float tSphere = -1.0f;
        float tFloor = -1.0f;
        float hitT = 0.0f;

        if (marchToSurface(ro, rayDir, boxDistOnly, timeSeconds, hitT))
        {
            tBox = hitT;
        }

        if (marchToSurface(ro, rayDir, sphereDistOnly, timeSeconds, hitT))
        {
            tSphere = hitT;
        }

        if (marchToSurface(ro, rayDir, floorDistOnly, timeSeconds, hitT))
        {
            tFloor = hitT;
        }

        if (tBox < 0.0f && tSphere < 0.0f)
        {
            return kPickNone;
        }

        float closestObjectT = 0.0f;
        if (tBox >= 0.0f && tSphere >= 0.0f)
        {
            closestObjectT = std::min(tBox, tSphere);
        }
        else if (tBox >= 0.0f)
        {
            closestObjectT = tBox;
        }
        else
        {
            closestObjectT = tSphere;
        }

        if (tFloor >= 0.0f && tFloor < closestObjectT)
        {
            return kPickNone;
        }

        if (tSphere >= 0.0f && (tBox < 0.0f || tSphere <= tBox))
        {
            return kPickSphere;
        }

        if (tBox >= 0.0f)
        {
            return kPickBox;
        }

        return kPickNone;
    }

    void processPendingPick(GLFWwindow* window, const int framebufferWidth, const int framebufferHeight)
    {
        if (!g_pickRequested || ImGui::GetIO().WantCaptureMouse)
        {
            g_pickRequested = false;
            return;
        }

        int windowWidth = framebufferWidth;
        int windowHeight = framebufferHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        if (windowWidth <= 0 || windowHeight <= 0)
        {
            g_pickRequested = false;
            return;
        }

        const float pickX = g_pickMouseX * (static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth));
        const float pickY = g_pickMouseY * (static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight));

        const Vec3 rayDir = buildCameraRay(g_camera, pickX, pickY, framebufferWidth, framebufferHeight);
        g_selectedObjectID = pickObjectAlongRay(g_camera, rayDir, g_timeSeconds);
        g_pickRequested = false;
    }

    void pushObjectButtonStyle(const float color[3], const bool selected)
    {
        if (!selected)
        {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color[0] * 0.55f + 0.25f, color[1] * 0.55f + 0.25f, color[2] * 0.55f + 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color[0] * 0.70f + 0.20f, color[1] * 0.70f + 0.20f, color[2] * 0.70f + 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color[0] * 0.85f + 0.10f, color[1] * 0.85f + 0.10f, color[2] * 0.85f + 0.10f, 1.0f));
    }

    void popObjectButtonStyle(const bool selected)
    {
        if (selected)
        {
            ImGui::PopStyleColor(3);
        }
    }

    void renderSceneUi()
    {
        ImGui::Begin("Scene");

        ImGui::Text("Animation");
        if (g_timePaused)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "Paused at t = %.2f", g_timeSeconds);
            if (ImGui::Button("Resume", ImVec2(120.0f, 0.0f)))
            {
                g_timePaused = false;
            }
        }
        else
        {
            ImGui::Text("Playing  (t = %.2f)", g_timeSeconds);
            if (ImGui::Button("Pause", ImVec2(120.0f, 0.0f)))
            {
                g_timePaused = true;
            }
        }

        ImGui::Separator();
        ImGui::Text("Objects");
        ImGui::TextDisabled("Click an object in the viewport or use the buttons below");

        const bool sphereSelected = g_selectedObjectID == kPickSphere;
        pushObjectButtonStyle(g_materials.sphere.color, sphereSelected);
        if (ImGui::Button("Sphere", ImVec2(-1.0f, 36.0f)))
        {
            g_selectedObjectID = kPickSphere;
        }
        popObjectButtonStyle(sphereSelected);
        if (sphereSelected)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.92f, 0.35f, 1.0f), "  * selected in scene");
        }

        const bool boxSelected = g_selectedObjectID == kPickBox;
        pushObjectButtonStyle(g_materials.box.color, boxSelected);
        if (ImGui::Button("Box", ImVec2(-1.0f, 36.0f)))
        {
            g_selectedObjectID = kPickBox;
        }
        popObjectButtonStyle(boxSelected);
        if (boxSelected)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.92f, 0.35f, 1.0f), "  * selected in scene");
        }

        ImGui::End();

        ImGui::Begin("Material Inspector");

        switch (g_selectedObjectID)
        {
        case kPickSphere:
            ImGui::Text("Editing: Sphere");
            ImGui::ColorEdit3("Color", g_materials.sphere.color);
            ImGui::SliderFloat("Roughness", &g_materials.sphere.roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Specular", &g_materials.sphere.specular, 0.0f, 1.0f);
            ImGui::SliderFloat("Metalness", &g_materials.sphere.metalness, 0.0f, 1.0f);
            break;
        case kPickBox:
            ImGui::Text("Editing: Box");
            ImGui::ColorEdit3("Color", g_materials.box.color);
            ImGui::SliderFloat("Roughness", &g_materials.box.roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Specular", &g_materials.box.specular, 0.0f, 1.0f);
            ImGui::SliderFloat("Metalness", &g_materials.box.metalness, 0.0f, 1.0f);
            break;
        default:
            ImGui::Text("No object selected");
            ImGui::TextDisabled("Click Sphere/Box in the viewport or use the Scene panel");
            break;
        }

        ImGui::End();
    }

    void framebufferSizeCallback(GLFWwindow* window, const int width, const int height)
    {
        (void)window;
        glViewport(0, 0, width, height);
    }

    void mouseCallback(GLFWwindow* window, const double xpos, const double ypos)
    {
        if (!g_isTrackingCamera)
        {
            return;
        }

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

    void mouseButtonCallback(GLFWwindow* window, const int button, const int action, const int mods)
    {
        (void)mods;

        Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
        if (camera == nullptr)
        {
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            if (action == GLFW_PRESS)
            {
                g_isTrackingCamera = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                double x = 0.0;
                double y = 0.0;
                glfwGetCursorPos(window, &x, &y);
                camera->lastX = static_cast<float>(x);
                camera->lastY = static_cast<float>(y);
                camera->firstMouse = true;
            }
            else if (action == GLFW_RELEASE)
            {
                g_isTrackingCamera = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !g_isTrackingCamera)
        {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(window, &x, &y);
            g_pickMouseX = static_cast<float>(x);
            g_pickMouseY = static_cast<float>(y);
            g_pickRequested = true;
        }
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
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSwapInterval(1);

        if (!initGlad())
        {
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

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

        float lastFrameTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(window))
        {
            const float currentFrameTime = static_cast<float>(glfwGetTime());
            const float deltaTime = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;
            if (!g_timePaused)
            {
                g_timeSeconds += deltaTime;
            }

            processInput(window, g_camera, deltaTime);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            int framebufferWidth = kWindowWidth;
            int framebufferHeight = kWindowHeight;
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

            renderSceneUi();
            processPendingPick(window, framebufferWidth, framebufferHeight);

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            raymarchShader.use();
            raymarchShader.setUniform("u_cameraPos", g_camera.posX, g_camera.posY, g_camera.posZ);
            raymarchShader.setUniform("u_cameraFront", g_camera.frontX, g_camera.frontY, g_camera.frontZ);
            raymarchShader.setUniform("u_cameraUp", g_camera.upX, g_camera.upY, g_camera.upZ);
            raymarchShader.setUniform(
                "u_resolution",
                static_cast<float>(framebufferWidth),
                static_cast<float>(framebufferHeight));
            raymarchShader.setUniform("u_time", g_timeSeconds);
            raymarchShader.setUniform(
                "u_sphereColor",
                g_materials.sphere.color[0],
                g_materials.sphere.color[1],
                g_materials.sphere.color[2]);
            raymarchShader.setUniform("u_sphereRoughness", g_materials.sphere.roughness);
            raymarchShader.setUniform("u_sphereSpecular", g_materials.sphere.specular);
            raymarchShader.setUniform("u_sphereMetalness", g_materials.sphere.metalness);
            raymarchShader.setUniform(
                "u_boxColor",
                g_materials.box.color[0],
                g_materials.box.color[1],
                g_materials.box.color[2]);
            raymarchShader.setUniform("u_boxRoughness", g_materials.box.roughness);
            raymarchShader.setUniform("u_boxSpecular", g_materials.box.specular);
            raymarchShader.setUniform("u_boxMetalness", g_materials.box.metalness);
            raymarchShader.setUniform("u_selectedObjectID", g_selectedObjectID);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

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
