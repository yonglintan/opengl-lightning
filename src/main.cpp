// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

// C++ standard headers
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

// OpenGL Math Library (GLM)
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Function Declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void generateLightning(std::vector<float> &vertices, glm::vec3 start, glm::vec3 end, int depth, float displacement);
void updateLightning();
void setupOpenGL();
void renderLightning(const glm::mat4 &view, const glm::mat4 &projection);
std::string generateLSystem(const std::string &axiom, int iterations);
void interpretLSystem(const std::string &lsystem, glm::vec3 origin, glm::vec3 baseDirection, float segmentLength,
                      float angleVariance, std::vector<float> &outVertices);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void initParticleSystem();
void emitParticlesAlongLightning();
void updateParticles(float deltaTime);
void renderParticles();
void setupGroundPlane();
void renderGroundPlane(const glm::mat4 &view, const glm::mat4 &projection);
void setupSticks();
void addStick();
void addStickControls();
void renderSticks(const glm::mat4 &view, const glm::mat4 &projection);
void updateStickPositions();
void buildUI();

// Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.3f, -0.8f, -0.5f)); // Example direction
glm::vec3 globalLightColor = glm::vec3(1.0f, 1.0f, 1.0f);                  // White light

unsigned int sceneFBO;

// Lightning parameters (adjustable via UI)
int maxDepth = 5;
float displacement = 0.5f;
glm::vec3 lightningColor = glm::vec3(1.0f, 1.0f, 1.0f);

struct Segment
{
    glm::vec3 start;
    glm::vec3 end;
};

// sub branching
float branchChance = 0.25f;  // Chance per segment (0.0 to 1.0)
int lsystemIterations = 3;   // Number of L-system iterations
float segmentLength = 0.1f;  // Length of each L-system segment
float angleVariance = 45.0f; // Max rotation in degrees

// Probabilities for L-system rules
float probFF = 0.5f; // probability of no branch
float probPlus = 0.3f;
float probMinus = 0.2f;

// Particle parameters
struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float size;
    float life;
    float maxLife;
};

struct Stick
{
    glm::vec3 position; // Base position on the ground
    float height;       // Height of the stick
    glm::vec3 color;    // Color of the stick
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

// Stick Global Variables
std::vector<Stick> sticks;
unsigned int stickVAO, stickVBO, stickEBO;
std::vector<unsigned int> stickIndices;
unsigned int stickShaderProgram;
glm::vec3 stickColor = glm::vec3(0.6f, 0.4f, 0.2f); // Brown color for sticks
int numSticks = 2;                                  // Start with two sticks: source and destination

// Particle Global Variables
std::vector<Particle> particles;
unsigned int particleVAO, particleVBO;
unsigned int particleShaderProgram;
unsigned int planeVAO, planeVBO;
unsigned int planeShaderProgram;
float particleEmissionRate = 0.05f;
float particleLifetime = 1.0f;

// OpenGL lightning objects
unsigned int VAO, VBO, lightningShaderProgram;
std::vector<float> lightningVertices;

// Camera settings
glm::vec3 cameraPos = glm::vec3(2.0f, 2.0f, 2.0f); // Diagonal view
glm::vec3 cameraFront = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -135.0f, pitch = -35.0f;
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool isDragging = false;

// Animation setting (just regenerate every frame for now)
bool isAutoRegenerating = false;

// Vertex Shader (MVP Transform)
const char *lightningVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Fragment Shader
const char *lightningFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 lightningColor;
void main()
{
    FragColor = vec4(lightningColor, 1.0);
}
)";

// Stick Vertex Shader
const char *stickVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;    // Vertex Position (object space)
    layout (location = 1) in vec3 aNormal; // Vertex Normal (object space)

    uniform mat4 model;      // Transforms object to world space
    uniform mat4 view;       // Transforms world to view space
    uniform mat4 projection; // Transforms view to clip space

    // Output to Fragment Shader
    out vec3 FragPos;       // Fragment's position in World Space
    out vec3 Normal;        // Fragment's normal in World Space

    void main()
    {
        // Calculate world position of the vertex
        FragPos = vec3(model * vec4(aPos, 1.0));

        // Calculate world normal: Use inverse transpose of model matrix's upper 3x3
        // This correctly handles non-uniform scaling (like scaling only height)
        Normal = mat3(transpose(inverse(model))) * aNormal;
        // We will normalize the Normal vector in the fragment shader after interpolation

        // Calculate final clip space position
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

// Stick Fragment Shader
const char *stickFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    // Input from Vertex Shader (interpolated)
    in vec3 FragPos;  // World space position of the fragment
    in vec3 Normal;   // World space normal of the fragment

    // Material Properties (passed from C++)
    uniform vec3 objectColor; // Base color of the stick
    uniform vec3 diffuseColor;
    uniform vec3 specularColor;
    uniform float shininess;

    // Light Properties (passed from C++)
    uniform vec3 lightDir;   // Direction *FROM* the light source (normalized)
    uniform vec3 lightColor; // Color of the light (e.g., white vec3(1.0))

    // Viewer Properties (passed from C++)
    uniform vec3 viewPos;    // Camera position in world space

    void main()
    {
        // --- Phong Lighting Calculation ---

        // Ambient component (constant low light)
        float ambientStrength = 0.15; // Can adjust
        vec3 ambient = ambientStrength * lightColor;

        // Diffuse component (light hitting the surface)
        vec3 norm = normalize(Normal);
        // Calculate direction TO the light source (reverse of lightDir)
        vec3 dirToLight = normalize(-lightDir);
        // Calculate diffuse intensity
        float diff = max(dot(norm, dirToLight), 0.0);
        vec3 diffuse = diffuseColor * diff * lightColor;

        // Specular component (shiny reflection)
        vec3 viewDir = normalize(viewPos - FragPos); // Direction from fragment to viewer
        // Calculate reflection direction using GLSL's reflect function
        // reflect expects incident vector (FROM light source)
        vec3 reflectDir = reflect(lightDir, norm);
        // Calculate specular intensity
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularColor * spec * lightColor; // Apply strength and light color

        // Combine components and multiply by object's base color
        vec3 result = (ambient + diffuse + specular) * objectColor;

        // Final color output (fully opaque)
        FragColor = vec4(result, 1.0);
    }
)";

// Particle Vertex Shader
const char *particleVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    layout (location = 2) in float aSize;

    uniform mat4 view;
    uniform mat4 projection;

    out vec3 Color;

    void main()
    {
        Color = aColor;
        gl_Position = projection * view * vec4(aPos, 1.0);
        gl_PointSize = aSize / gl_Position.z;
    }
    )";

// Particle Fragment Shader
const char *particleFragmentShaderSource = R"(
    #version 330 core
    in vec3 Color;
    out vec4 FragColor;

    void main()
    {
        // Create a circular particle
        vec2 coord = gl_PointCoord - vec2(0.5);
        if(length(coord) > 0.5)
            discard;

        // Fade out towards edges
        float alpha = 1.0 - smoothstep(0.3, 0.5, length(coord));
        FragColor = vec4(Color, alpha);
    }
    )";

const char *planeVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
    )";

const char *planeFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 planeColor;
    void main()
    {
        FragColor = vec4(planeColor, 1.0);
    }
    )";

unsigned int compileAndLinkShaders(const char *vertexShaderSource, const char *fragmentShaderSource)
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Procedural Lightning", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    lightningShaderProgram = compileAndLinkShaders(lightningVertexShaderSource, lightningFragmentShaderSource);
    setupOpenGL();
    setupGroundPlane();
    setupSticks();
    initParticleSystem();
    updateLightning();
    emitParticlesAlongLightning(); // Initial particles

    float lastFrame = 0.0f;
    float currentFrame = 0.0f;

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (!glfwWindowShouldClose(window))
    {
        // Calculate delta time
        currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Update particles
        updateParticles(deltaTime);

        // Emit particles regularly for constant effect
        static float particleTimer = 0.0f;
        particleTimer += deltaTime;
        if (particleTimer > particleEmissionRate)
        {
            emitParticlesAlongLightning();
            particleTimer = 0.0f;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

        if (isAutoRegenerating)
        {
            updateLightning(); // Regenerate every frame
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderGroundPlane(view, projection);
        renderSticks(view, projection);
        renderLightning(view, projection);
        renderParticles();

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        buildUI();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void buildUI()
{
    ImGui::Begin("Lightning Settings");
    ImGui::SliderInt("Max Depth", &maxDepth, 1, 10);
    ImGui::SliderFloat("Displacement", &displacement, 0.0f, 5.0f);
    ImGui::ColorEdit3("Color", glm::value_ptr(lightningColor), ImGuiColorEditFlags_NoInputs);

    // Add Branch controls
    ImGui::Separator();
    ImGui::Text("Lightning Branches");
    ImGui::SliderFloat("Branch Length", &segmentLength, 0.0f, 0.15f);
    ImGui::SliderFloat("Branch Frequncy", &branchChance, 0.0f, 1.0f);

    // Add particle controls
    ImGui::Separator();
    ImGui::Text("Particle Effects");
    ImGui::SliderFloat("Emission Rate", &particleEmissionRate, 0.01f, 0.2f);
    ImGui::SliderFloat("Particle Lifetime", &particleLifetime, 0.1f, 2.0f);

    addStickControls();

    if (ImGui::Button(isAutoRegenerating ? "Stop" : "Play"))
    {
        isAutoRegenerating = !isAutoRegenerating; // Toggle state
    }
    ImGui::SameLine();

    if (ImGui::Button("Regenerate"))
    {
        updateLightning();
        emitParticlesAlongLightning();
    }
    ImGui::SameLine();

    if (ImGui::Button("Add Stick"))
    {
        addStick();
    }

    ImGui::End();
}

void generateLightning(std::vector<float> &vertices, glm::vec3 start, glm::vec3 end, int depth, float displacement)
{
    if (depth == 0)
    {
        // Add the start position
        vertices.push_back(start.x);
        vertices.push_back(start.y);
        vertices.push_back(start.z);

        // Also add the end position to ensure the lightning connects fully
        vertices.push_back(end.x);
        vertices.push_back(end.y);
        vertices.push_back(end.z);

        // Generate branches using L-systems
        float r = float(rand()) / float(RAND_MAX);
        if (r > branchChance)
        {
            // Do not generate branches
        }
        else if (r < probPlus / probFF * branchChance)
        {
            std::string lsystem = generateLSystem("[+F]", lsystemIterations);
            interpretLSystem(lsystem, start, glm::normalize(end - start), segmentLength, angleVariance, vertices);
        }
        else
        {
            std::string lsystem = generateLSystem("[-F]", lsystemIterations);
            interpretLSystem(lsystem, start, glm::normalize(end - start), segmentLength, angleVariance, vertices);
        }

        return;
    }

    glm::vec3 mid = (start + end) * 0.5f;
    mid.y += (float(rand()) / float(RAND_MAX) - 0.5f) * displacement;
    mid.z += (float(rand()) / float(RAND_MAX) - 0.5f) * displacement;

    generateLightning(vertices, start, mid, depth - 1, displacement * 0.5f);
    generateLightning(vertices, mid, end, depth - 1, displacement * 0.5f);
}

void updateLightning()
{
    if (sticks.size() < 2)
        return;

    lightningVertices.clear();
    std::vector<Segment> mainSegments;

    // Generate lightning between sticks
    for (size_t i = 0; i < sticks.size() - 1; ++i)
    {
        glm::vec3 startPos = {sticks[i].position.x, sticks[i].position.y + sticks[i].height, sticks[i].position.z};

        glm::vec3 endPos = {sticks[i + 1].position.x, sticks[i + 1].position.y + sticks[i + 1].height,
                            sticks[i + 1].position.z};

        generateLightning(lightningVertices, startPos, endPos, maxDepth, displacement);
    }

    // Upload to GPU
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, lightningVertices.size() * sizeof(float), lightningVertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void renderLightning(const glm::mat4 &view, const glm::mat4 &projection)
{
    glUseProgram(lightningShaderProgram);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(lightningShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(lightningShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(lightningShaderProgram, "projection"), 1, GL_FALSE,
                       glm::value_ptr(projection));

    // Render multiple passes for a glow effect
    glBindVertexArray(VAO);

    // Main lightning (thick and bright)
    glLineWidth(1.0f);
    glUniform3fv(glGetUniformLocation(lightningShaderProgram, "lightningColor"), 1, glm::value_ptr(lightningColor));
    glDrawArrays(GL_LINES, 0, lightningVertices.size() / 3);

    // Glow effect (slightly transparent and thinner)
    glLineWidth(5.0f);
    glm::vec3 glowColor = lightningColor * 1.5f;
    glUniform3fv(glGetUniformLocation(lightningShaderProgram, "lightningColor"), 1, glm::value_ptr(glowColor));
    glDrawArrays(GL_LINES, 0, lightningVertices.size() / 3);

    // Core lightning (very bright and thin)
    glLineWidth(2.0f);
    glm::vec3 coreColor = lightningColor * 2.0f;
    glUniform3fv(glGetUniformLocation(lightningShaderProgram, "lightningColor"), 1, glm::value_ptr(coreColor));
    glDrawArrays(GL_LINES, 0, lightningVertices.size() / 3);

    glBindVertexArray(0);
}

//----------------L system subbranching--------------------//
std::string generateLSystem(const std::string &axiom, int iterations)
{
    std::string result = axiom;

    for (int i = 0; i < iterations; ++i)
    {
        std::string next;
        for (char c : result)
        {
            if (c == 'F')
            {
                float r = float(rand()) / float(RAND_MAX);
                if (r < probFF)
                {
                    next += "FF";
                }
                else if (r < probFF + probPlus)
                {
                    next += "F[+F]";
                }
                else
                {
                    next += "F[-F]";
                }
            }
            else
            {
                next += c;
            }
        }
        result = next;
    }

    return result;
}

void interpretLSystem(const std::string &lsystem, glm::vec3 origin, glm::vec3 baseDirection, float segmentLength,
                      float angleVariance, std::vector<float> &outVertices)
{
    struct State
    {
        glm::vec3 pos;
        glm::vec3 dir;
    };

    std::stack<State> stateStack;
    glm::vec3 currentPos = origin;
    glm::vec3 currentDir = glm::normalize(baseDirection);

    for (char c : lsystem)
    {
        switch (c)
        {
        case 'F':
        {
            glm::vec3 nextPos = currentPos + currentDir * segmentLength;
            outVertices.push_back(currentPos.x);
            outVertices.push_back(currentPos.y);
            outVertices.push_back(currentPos.z);
            outVertices.push_back(nextPos.x);
            outVertices.push_back(nextPos.y);
            outVertices.push_back(nextPos.z);
            currentPos = nextPos;
            break;
        }
        case '+':
        {
            float angle = angleVariance * ((float(rand()) / RAND_MAX));
            currentDir = glm::rotate(currentDir, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
            break;
        }
        case '-':
        {
            float angle = angleVariance * ((float(rand()) / RAND_MAX));
            currentDir = glm::rotate(currentDir, glm::radians(-angle), glm::vec3(0.0f, 0.0f, 1.0f));
            break;
        }
        case '[':
            stateStack.push({currentPos, currentDir});
            break;
        case ']':
            if (!stateStack.empty())
            {
                currentPos = stateStack.top().pos;
                currentDir = stateStack.top().dir;
                stateStack.pop();
            }
            break;
        }
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (!isDragging)
        {
            isDragging = true;
            lastX = xpos;
            lastY = ypos;
            return; // Prevent a sudden jump in camera angle
        }

        float xOffset = xpos - lastX;
        float yOffset = lastY - ypos; // Inverted Y

        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.1f;
        yaw += xOffset * sensitivity;
        pitch += yOffset * sensitivity;

        // Constrain pitch to prevent flipping
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        // Update camera direction
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(direction);
    }
    else
    {
        isDragging = false; // Reset dragging state when mouse is released
    }
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    const float cameraSpeed = 0.004f;
    const float zoomModifier = 2.0f;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos += cameraSpeed * zoomModifier * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos -= cameraSpeed * zoomModifier * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

void setupOpenGL()
{
    glEnable(GL_DEPTH_TEST);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void initParticleSystem()
{
    // Compile particle shaders
    particleShaderProgram = compileAndLinkShaders(particleVertexShaderSource, particleFragmentShaderSource);

    // Create VAO and VBO for particles
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void *)0);

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void *)offsetof(Particle, color));

    // Size attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (void *)offsetof(Particle, size));

    glBindVertexArray(0);
}

void emitParticlesAlongLightning()
{
    // Emit particles along the lightning path
    for (size_t i = 0; i < lightningVertices.size(); i += 3)
    {
        // Skip some vertices to avoid too many particles
        if (rand() % 3 != 0)
            continue;

        glm::vec3 position(lightningVertices[i], lightningVertices[i + 1], lightningVertices[i + 2]);

        // Random direction
        float randomAngle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f * 3.14159f;
        float randomSpeed = 0.02f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 0.05f;

        Particle particle;
        particle.position = position;
        particle.velocity = glm::vec3(cos(randomAngle) * randomSpeed, sin(randomAngle) * randomSpeed,
                                      (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 0.05f);

        // Slight color variation
        float colorVariation = 0.85f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 0.15f;
        particle.color = lightningColor * colorVariation;

        // Random size and life
        particle.maxLife = particleLifetime * (0.5f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
        particle.life = particle.maxLife;

        particles.push_back(particle);
    }
}

void updateParticles(float deltaTime)
{
    // Update existing particles
    for (auto it = particles.begin(); it != particles.end();)
    {
        it->life -= deltaTime;
        if (it->life <= 0.0f)
        {
            it = particles.erase(it);
        }
        else
        {
            it->position += it->velocity * deltaTime;
            it->size -= deltaTime * 0.5f;

            // Fade color as life decreases
            float lifeRatio = it->life / it->maxLife;
            it->color = lightningColor * lifeRatio;

            ++it;
        }
    }
}

void renderParticles()
{
    if (particles.empty())
        return;

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glUseProgram(particleShaderProgram);

    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);

    glUniformMatrix4fv(glGetUniformLocation(particleShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(particleShaderProgram, "projection"), 1, GL_FALSE,
                       glm::value_ptr(projection));

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_POINTS, 0, particles.size());

    glBindVertexArray(0);
    glUseProgram(0);

    glDisable(GL_BLEND);
}

void setupGroundPlane()
{
    // Plane vertices (a simple square)
    float planeVertices[] = {
        // positions
        -5.0f, -1.0f, -5.0f, // Corner 1
        5.0f,  -1.0f, -5.0f, // Corner 2
        5.0f,  -1.0f, 5.0f,  // Corner 3
        -5.0f, -1.0f, 5.0f   // Corner 4
    };

    unsigned int planeIndices[] = {
        0, 1, 2, // First triangle
        0, 2, 3, // Second triangle
    };

    // Create shader program
    planeShaderProgram = compileAndLinkShaders(planeVertexShaderSource, planeFragmentShaderSource);

    // Create VAO/VBO
    unsigned int planeEBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glGenBuffers(1, &planeEBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices), planeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindVertexArray(0);
}

void renderGroundPlane(const glm::mat4 &view, const glm::mat4 &projection)
{
    glUseProgram(planeShaderProgram);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(planeShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(planeShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(planeShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Dark gray color for the plane
    glUniform3f(glGetUniformLocation(planeShaderProgram, "planeColor"), 0.2f, 0.2f, 0.2f);

    glBindVertexArray(planeVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void generateCylinderMesh(float radius, float height, int segments, std::vector<Vertex> &vertices,
                          std::vector<unsigned int> &indices)
{
    // Top center vertex
    vertices.push_back({glm::vec3(0, height, 0), glm::vec3(0, 1, 0)});
    int topCenterIndex = 0;

    // Top circle
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glm::vec3 pos = glm::vec3(x, height, z);
        vertices.push_back({pos, glm::vec3(0, 1, 0)});
    }

    // Bottom center vertex
    int bottomCenterIndex = vertices.size();
    vertices.push_back({glm::vec3(0, 0, 0), glm::vec3(0, -1, 0)});

    // Bottom circle
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glm::vec3 pos = glm::vec3(x, 0, z);
        vertices.push_back({pos, glm::vec3(0, -1, 0)});
    }

    // Side surface
    int sideStartIndex = vertices.size();
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glm::vec3 normal = glm::normalize(glm::vec3(x, 0, z));

        glm::vec3 topPos = glm::vec3(x, height, z);
        glm::vec3 bottomPos = glm::vec3(x, 0, z);

        vertices.push_back({topPos, normal});
        vertices.push_back({bottomPos, normal});
    }

    // Top cap triangle fan indices
    for (int i = 1; i <= segments; ++i)
    {
        indices.push_back(topCenterIndex);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    // Bottom cap triangle fan indices
    for (int i = 1; i <= segments; ++i)
    {
        indices.push_back(bottomCenterIndex);
        indices.push_back(bottomCenterIndex + i + 1);
        indices.push_back(bottomCenterIndex + i);
    }

    // Side triangle strips indices
    for (int i = 0; i < segments; ++i)
    {
        int top = sideStartIndex + i * 2;
        int bottom = top + 1;
        int nextTop = top + 2;
        int nextBottom = bottom + 2;

        // Triangle 1
        indices.push_back(top);
        indices.push_back(bottom);
        indices.push_back(nextTop);

        // Triangle 2
        indices.push_back(nextTop);
        indices.push_back(bottom);
        indices.push_back(nextBottom);
    }
}

void setupSticks()
{
    // Create initial sticks
    sticks.clear();

    // First stick - source
    Stick sourceStick;
    sourceStick.position = glm::vec3(-1.5f, -1.0f, 0.0f); // On the ground plane
    sourceStick.height = 2.0f;
    sourceStick.color = stickColor;
    sticks.push_back(sourceStick);

    // Second stick - destination
    Stick destStick;
    destStick.position = glm::vec3(1.5f, -1.0f, 0.0f); // On the ground plane
    destStick.height = 1.5f;
    destStick.color = stickColor;
    sticks.push_back(destStick);

    std::vector<Vertex> cylinderVertices;

    generateCylinderMesh(0.02f, 1.0f, 10, cylinderVertices, stickIndices);

    // Create VAO, VBO, and EBO for sticks
    glGenBuffers(1, &stickVBO);
    glGenBuffers(1, &stickEBO);
    glGenVertexArrays(1, &stickVAO);

    // Configure VAO
    glBindVertexArray(stickVAO);

    glBindBuffer(GL_ARRAY_BUFFER, stickVBO);
    glBufferData(GL_ARRAY_BUFFER, cylinderVertices.size() * sizeof(Vertex), cylinderVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stickEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, stickIndices.size() * sizeof(unsigned int), stickIndices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Create shader program
    stickShaderProgram = compileAndLinkShaders(stickVertexShaderSource, stickFragmentShaderSource);
}

void addStick()
{
    Stick newStick;
    newStick.position = glm::vec3(0.0f, -1.0f, 0.0f);
    newStick.height = 2.0f;
    newStick.color = stickColor;
    sticks.push_back(newStick);
}

void renderSticks(const glm::mat4 &view, const glm::mat4 &projection)
{
    glUseProgram(stickShaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(stickShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(stickShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform3fv(glGetUniformLocation(stickShaderProgram, "lightDir"), 1, glm::value_ptr(lightDirection));
    glUniform3fv(glGetUniformLocation(stickShaderProgram, "lightColor"), 1, glm::value_ptr(globalLightColor));
    glUniform3fv(glGetUniformLocation(stickShaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

    glBindVertexArray(stickVAO);

    for (const auto &stick : sticks)
    {
        // Create model matrix for each stick by translating to stick position and scaling to stick height
        glm::mat4 model = glm::translate(glm::mat4(1.0f), stick.position);
        model = glm::scale(model, glm::vec3(1.0f, stick.height, 1.0f));
        glUniformMatrix4fv(glGetUniformLocation(stickShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // Set stick color
        glUniform3fv(glGetUniformLocation(stickShaderProgram, "objectColor"), 1, glm::value_ptr(stick.color));
        glUniform3fv(glGetUniformLocation(stickShaderProgram, "diffuseColor"), 1,
                     glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
        glUniform3fv(glGetUniformLocation(stickShaderProgram, "specularColor"), 1,
                     glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
        glUniform1f(glGetUniformLocation(stickShaderProgram, "shininess"), 1.0f);

        // Draw stick as cylinder
        glDrawElements(GL_TRIANGLES, stickIndices.size(), GL_UNSIGNED_INT, 0);

        // TODO: Lightning effect on the stick. Edit/create different shader if required.
    }

    glBindVertexArray(0);
}

void addStickControls()
{
    ImGui::Separator();
    ImGui::Text("Stick Settings");

    // Common stick settings
    if (ImGui::ColorEdit3("Stick Color", glm::value_ptr(stickColor), ImGuiColorEditFlags_NoInputs))
    {
        // Update all stick colors when changed
        for (auto &stick : sticks)
        {
            stick.color = stickColor;
        }
    }
    // Start stick settings
    if (ImGui::CollapsingHeader("Stick Positions", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Column headers
        ImGui::Text(" ");
        ImGui::SameLine(0, 55); // Spacer
        ImGui::Text("Height");
        ImGui::SameLine(0, 60);
        ImGui::Text("X");
        ImGui::SameLine(0, 90);
        ImGui::Text("Z");

        for (int i = 0; i < sticks.size(); ++i)
        {
            auto &stick = sticks[i];
            bool startChanged = false;

            // Row label (e.g., Stick 1)
            ImGui::Text("Stick %d", i + 1);
            ImGui::SameLine(0, 10);

            // Set slider width
            ImGui::PushItemWidth(80.0f);

            // Height slider
            startChanged |= ImGui::SliderFloat(("##Height" + std::to_string(i)).c_str(), &stick.height, 0.5f, 3.0f);
            ImGui::SameLine(0, 20);

            // X slider
            startChanged |= ImGui::SliderFloat(("##X" + std::to_string(i)).c_str(), &stick.position.x, -4.5f, 4.5f);
            ImGui::SameLine(0, 20);

            // Z slider
            startChanged |= ImGui::SliderFloat(("##Z" + std::to_string(i)).c_str(), &stick.position.z, -4.5f, 4.5f);

            ImGui::PopItemWidth();

            if (startChanged)
            {
                updateLightning();
            }
        }
        ImGui::Separator();
    }
}
