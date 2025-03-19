#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib> // for rand()

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void generateLightning(std::vector<float>& vertices, float x1, float y1, float x2, float y2, int depth, float displacement);
void updateLightning();
void setupOpenGL();
void renderLightning();

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Lightning parameters (adjustable via UI)
int maxDepth = 5;             // Adjustable recursion depth
float displacement = 0.5f;   // Adjustable max displacement

// OpenGL objects
unsigned int VAO, VBO;
std::vector<float> lightningVertices;

int main()
{
    // GLFW: Initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // GLFW Window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Procedural Lightning with UI", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // GLAD: Load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Setup OpenGL buffers
    setupOpenGL();
    updateLightning();

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui UI: Controls
        ImGui::Begin("Lightning Settings");
        ImGui::SliderInt("Max Depth", &maxDepth, 1, 10);
        ImGui::SliderFloat("Displacement", &displacement, 0.0f, 1.0f);
        if (ImGui::Button("Regenerate"))
        {
            updateLightning();
        }
        ImGui::End();

        // Clear screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw Lightning
        renderLightning();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glfwTerminate();
    return 0;
}

// Recursively subdivides a line segment with random displacement
void generateLightning(std::vector<float>& vertices, float x1, float y1, float x2, float y2, int depth, float displacement)
{
    if (depth == 0)
    {
        vertices.push_back(x1);
        vertices.push_back(y1);
        return;
    }

    // Midpoint displacement
    float midX = (x1 + x2) / 2.0f;
    float midY = (y1 + y2) / 2.0f;
    midY += (float(rand()) / RAND_MAX - 0.5f) * 2.0f * displacement;

    // Recursively subdivide
    generateLightning(vertices, x1, y1, midX, midY, depth - 1, displacement * 0.5f);
    generateLightning(vertices, midX, midY, x2, y2, depth - 1, displacement * 0.5f);
}

// Updates the lightning bolt data
void updateLightning()
{
    lightningVertices.clear();
    generateLightning(lightningVertices, -0.5f, 0.8f, 0.5f, -0.8f, maxDepth, displacement);
    lightningVertices.push_back(0.5f);
    lightningVertices.push_back(-0.8f);

    // Upload new data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, lightningVertices.size() * sizeof(float), lightningVertices.data(), GL_DYNAMIC_DRAW);
}

// Sets up OpenGL buffers for rendering
void setupOpenGL()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Renders the lightning bolt
void renderLightning()
{
    glBindVertexArray(VAO);
    glDrawArrays(GL_LINE_STRIP, 0, lightningVertices.size() / 2);
    glBindVertexArray(0);
}

// process input
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
}

// glfw: whenever the window size changed
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}
