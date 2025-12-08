#include <GL/glew.h>
#include <gl/freeglut.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <random>
#include <ctime>
#include <numeric>
#include <algorithm>
#include <fstream>

int g_windowWidth = 1024;
int g_windowHeight = 768;
int g_gridWidth = 21;
int g_gridHeight = 21;
const float CUBE_SIZE = 0.8f;
const float GRID_SPACING = 0.2f;

GLuint g_shaderProgram = 0;
GLuint g_cubeVAO = 0, g_cubeVBO = 0, g_cubeEBO = 0;
GLint g_modelLoc = -1, g_viewLoc = -1, g_projLoc = -1, g_colorLoc = -1;

glm::vec3 g_cameraPos = glm::vec3(0.0f, 10.0f, 15.0f);
glm::vec3 g_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 g_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float g_playerPosX = 0.0f;
float g_playerPosZ = 0.0f;
float g_playerAngleY = 0.0f;
int g_mazeStartX = 0;
int g_mazeEndX = 0;

const float PLAYER_WIDTH = 0.3f;
const float PLAYER_HEIGHT = 0.5f;
const float PLAYER_DEPTH = 0.3f;
const float PLAYER_MOVE_SPEED = 2.0f;
bool g_keyStates[256];
bool g_specialKeyStates[128];
const float GRID_BASE_SCALE = 1.0f;
const float WALL_SCALE = 2.0f;
const float FLOOR_SCALE = 0.05f;

std::vector<std::vector<float>> g_cubeCurrentHeight;
std::vector<std::vector<float>> g_cubeCurrentScale;

enum CellType { WALL, PATH };
std::vector<std::vector<CellType>> g_maze;

std::mt19937 g_randomEngine;
int g_lastTime = 0;

std::string readShaderSource(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "̴    ϴ: " << filePath << std::endl;
        return "";
    }
    std::stringstream stream;
    stream << file.rdbuf();
    file.close();
    return stream.str();
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "̴  : " << infoLog << std::endl;
        return 0;
    }
    return shader;
}

GLuint createShaderProgram(const char* vsSource, const char* fsSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "α׷ ũ : " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

glm::vec3 getWorldPos(int gridX, int gridZ) {
    float totalGridWidth = (g_gridWidth - 1) * (CUBE_SIZE + GRID_SPACING);
    float totalGridHeight = (g_gridHeight - 1) * (CUBE_SIZE + GRID_SPACING);
    float startX = -totalGridWidth / 2.0f;
    float startZ = -totalGridHeight / 2.0f;
    float x = startX + gridX * (CUBE_SIZE + GRID_SPACING);
    float z = startZ + gridZ * (CUBE_SIZE + GRID_SPACING);
    return glm::vec3(x, 0.0f, z);
}

glm::ivec2 getGridCoord(float worldX, float worldZ) {
    float totalGridWidth = (g_gridWidth - 1) * (CUBE_SIZE + GRID_SPACING);
    float totalGridHeight = (g_gridHeight - 1) * (CUBE_SIZE + GRID_SPACING);
    float startX = -totalGridWidth / 2.0f;
    float startZ = -totalGridHeight / 2.0f;
    float unitSize = CUBE_SIZE + GRID_SPACING;
    int gridX = (int)glm::round((worldX - startX) / unitSize);
    int gridZ = (int)glm::round((worldZ - startZ) / unitSize);
    return glm::ivec2(gridX, gridZ);
}

void generateMaze(int x, int z) {
    g_maze[z][x] = PATH;

    int dx[] = { 0, 0, 1, -1 };
    int dz[] = { 1, -1, 0, 0 };
    std::vector<int> directions = { 0, 1, 2, 3 };
    std::shuffle(directions.begin(), directions.end(), g_randomEngine);

    for (int i = 0; i < 4; ++i) {
        int dir = directions[i];
        int nx = x + dx[dir] * 2;
        int nz = z + dz[dir] * 2;

        if (nx > 0 && nx < g_gridWidth - 1 && nz > 0 && nz < g_gridHeight - 1) {
            if (g_maze[nz][nx] == WALL) {
                g_maze[z + dz[dir]][x + dx[dir]] = PATH;
                generateMaze(nx, nz);
            }
        }
    }
}

void initCubes() {
    g_maze.resize(g_gridHeight, std::vector<CellType>(g_gridWidth));
    g_cubeCurrentHeight.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_cubeCurrentScale.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_randomEngine.seed(static_cast<unsigned int>(std::time(0)));
}

void reset() {
    g_cameraPos = glm::vec3(0.0f, 10.0f, 15.0f);
    g_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    g_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    g_playerAngleY = 0.0f;

    for (int i = 0; i < 256; i++) g_keyStates[i] = false;
    for (int i = 0; i < 128; i++) g_specialKeyStates[i] = false;

    initCubes();

    g_maze.assign(g_gridHeight, std::vector<CellType>(g_gridWidth, WALL));
    int range = (g_gridWidth - 3) / 2;
    if (range < 0) range = 0;
    std::uniform_int_distribution<int> xDist(0, range);
    g_mazeStartX = xDist(g_randomEngine) * 2 + 1;
    g_mazeEndX = xDist(g_randomEngine) * 2 + 1;
    generateMaze(g_mazeEndX, g_gridHeight - 2);
    g_maze[0][g_mazeStartX] = PATH;
    g_maze[1][g_mazeStartX] = PATH;
    g_maze[g_gridHeight - 1][g_mazeEndX] = PATH;

    glm::vec3 playerStartPos = getWorldPos(g_mazeStartX, 0);
    g_playerPosX = playerStartPos.x;
    g_playerPosZ = playerStartPos.z;

    for (int i = 0; i < g_gridHeight; ++i) {
        for (int j = 0; j < g_gridWidth; ++j) {
            if (g_maze[i][j] == WALL) {
                g_cubeCurrentScale[i][j] = WALL_SCALE;
            }
            else {
                g_cubeCurrentScale[i][j] = FLOOR_SCALE;
            }
            g_cubeCurrentHeight[i][j] = (g_cubeCurrentScale[i][j] * CUBE_SIZE) / 2.0f;
        }
    }
}

void init() {
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {}

    std::string vsCode = readShaderSource("vertex.glsl");
    std::string fsCode = readShaderSource("fragment.glsl");

    if (vsCode.empty() || fsCode.empty()) {
        std::cerr << "̴ ҽ  ֽϴ.  θ Ȯϼ." << std::endl;
        exit(EXIT_FAILURE);
    }

    g_shaderProgram = createShaderProgram(vsCode.c_str(), fsCode.c_str());
    if (g_shaderProgram == 0) exit(EXIT_FAILURE);

    g_modelLoc = glGetUniformLocation(g_shaderProgram, "model");
    g_viewLoc = glGetUniformLocation(g_shaderProgram, "view");
    g_projLoc = glGetUniformLocation(g_shaderProgram, "projection");
    g_colorLoc = glGetUniformLocation(g_shaderProgram, "objectColor");

    float s = 0.5f;
    GLfloat vertices[] = { -s, -s,  s,  s, -s,  s,  s,  s,  s, -s,  s,  s, -s, -s, -s,  s, -s, -s,  s,  s, -s, -s,  s, -s };
    GLuint indices[] = { 3, 2, 6, 6, 7, 3, 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 4, 0, 3, 3, 7, 4, 1, 5, 6, 6, 2, 1, 0, 1, 5, 5, 4, 0 };
    glGenVertexArrays(1, &g_cubeVAO); glGenBuffers(1, &g_cubeVBO); glGenBuffers(1, &g_cubeEBO);
    glBindVertexArray(g_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    g_lastTime = glutGet(GLUT_ELAPSED_TIME);
    reset();
}

void drawCube() {
    glBindVertexArray(g_cubeVAO);
    glUniform3f(g_colorLoc, 1.0f, 1.0f, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(0));
    glUniform3f(g_colorLoc, 0.5f, 0.5f, 0.5f);
    glDrawElements(GL_TRIANGLES, 30, GL_UNSIGNED_INT, (void*)(6 * sizeof(GLuint)));
}

void drawGrid(glm::mat4 view, glm::mat4 projection) {
    glUseProgram(g_shaderProgram);
    glUniformMatrix4fv(g_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    float totalGridWidth = (g_gridWidth - 1) * (CUBE_SIZE + GRID_SPACING);
    float totalGridHeight = (g_gridHeight - 1) * (CUBE_SIZE + GRID_SPACING);
    float startX = -totalGridWidth / 2.0f;
    float startZ = -totalGridHeight / 2.0f;

    for (int i = 0; i < g_gridHeight; ++i) {
        for (int j = 0; j < g_gridWidth; ++j) {
            float x = startX + j * (CUBE_SIZE + GRID_SPACING);
            float z = startZ + i * (CUBE_SIZE + GRID_SPACING);
            float y = g_cubeCurrentHeight[i][j];
            float scaleFactorY = g_cubeCurrentScale[i][j];
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, y, z));
            model = glm::scale(model, glm::vec3(CUBE_SIZE, CUBE_SIZE * scaleFactorY, CUBE_SIZE));
            glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            drawCube();
        }
    }

    glm::ivec2 gridPos = getGridCoord(g_playerPosX, g_playerPosZ);
    float tileY = 0.0f;
    float tileScale = 0.0f;
    if (gridPos.y >= 0 && gridPos.y < g_gridHeight && gridPos.x >= 0 && gridPos.x < g_gridWidth) {
        tileY = g_cubeCurrentHeight[gridPos.y][gridPos.x];
        tileScale = g_cubeCurrentScale[gridPos.y][gridPos.x];
    }
    float playerDrawY = tileY + (tileScale * CUBE_SIZE / 2.0f) + (PLAYER_HEIGHT / 2.0f);
    glm::vec3 playerWorldPos = glm::vec3(g_playerPosX, playerDrawY, g_playerPosZ);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, playerWorldPos);
    model = glm::rotate(model, glm::radians(g_playerAngleY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_DEPTH));
    glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(g_cubeVAO);
    glUniform3f(g_colorLoc, 0.2f, 0.5f, 1.0f);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(0));
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, g_windowWidth, g_windowHeight);
    glm::mat4 view(1.0f);

    glm::ivec2 gridPos = getGridCoord(g_playerPosX, g_playerPosZ);
    float tileY = 0.0f;
    if (gridPos.y >= 0 && gridPos.y < g_gridHeight && gridPos.x >= 0 && gridPos.x < g_gridWidth) {
        tileY = g_cubeCurrentHeight[gridPos.y][gridPos.x];
    }
    glm::vec3 playerWorldPos = glm::vec3(g_playerPosX, tileY, g_playerPosZ);
    float angleRad = glm::radians(g_playerAngleY);
    glm::vec3 forward(sin(angleRad), 0.0f, cos(angleRad));
    glm::vec3 camOffset = -forward * 6.0f + glm::vec3(0.0f, 8.0f, 0.0f);
    g_cameraPos = playerWorldPos + camOffset;
    g_cameraTarget = playerWorldPos + glm::vec3(0.0f, 1.0f, 0.0f);
    view = glm::lookAt(g_cameraPos, g_cameraTarget, g_cameraUp);

    glm::mat4 projection(1.0f);
    float aspect = (float)g_windowWidth / g_windowHeight;
    projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    drawGrid(view, projection);

    int minimapSizeX = g_windowWidth / 4;
    int minimapSizeY = minimapSizeX / aspect;
    int padding = 10;
    glEnable(GL_SCISSOR_TEST);
    glScissor(g_windowWidth - minimapSizeX - padding, g_windowHeight - minimapSizeY - padding, minimapSizeX, minimapSizeY);
    glViewport(g_windowWidth - minimapSizeX - padding, g_windowHeight - minimapSizeY - padding, minimapSizeX, minimapSizeY);
    glClear(GL_DEPTH_BUFFER_BIT);
    glm::mat4 minimapView = glm::lookAt(glm::vec3(0.0f, 15.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    float minimapAspect = (float)minimapSizeX / (float)minimapSizeY;
    glm::mat4 minimapProj = glm::perspective(glm::radians(45.0f), minimapAspect, 0.1f, 100.0f);
    drawGrid(minimapView, minimapProj);
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glutSwapBuffers();
}

void reshape(int w, int h) {
    g_windowWidth = w;
    g_windowHeight = h;
}

void handlePlayerInput(float deltaTime) {
    glm::vec3 moveVector(0.0f, 0.0f, 0.0f);

    if (g_specialKeyStates[GLUT_KEY_UP] || g_keyStates['w'] || g_keyStates['W'])    moveVector += glm::vec3(0.0f, 0.0f, 1.0f);
    if (g_specialKeyStates[GLUT_KEY_DOWN] || g_keyStates['s'] || g_keyStates['S'])  moveVector -= glm::vec3(0.0f, 0.0f, 1.0f);
    if (g_specialKeyStates[GLUT_KEY_LEFT] || g_keyStates['a'] || g_keyStates['A'])  moveVector -= glm::vec3(1.0f, 0.0f, 0.0f);
    if (g_specialKeyStates[GLUT_KEY_RIGHT] || g_keyStates['d'] || g_keyStates['D']) moveVector += glm::vec3(1.0f, 0.0f, 0.0f);

    if (glm::length(moveVector) > 0.0f) {
        moveVector = glm::normalize(moveVector) * PLAYER_MOVE_SPEED * deltaTime;
        float newX = g_playerPosX + moveVector.x;
        float newZ = g_playerPosZ + moveVector.z;

        glm::ivec2 gridPos = getGridCoord(newX, g_playerPosZ);
        if (gridPos.x >= 0 && gridPos.x < g_gridWidth && gridPos.y >= 0 && gridPos.y < g_gridHeight && g_maze[gridPos.y][gridPos.x] == PATH) {
            g_playerPosX = newX;
        }
        gridPos = getGridCoord(g_playerPosX, newZ);
        if (gridPos.x >= 0 && gridPos.x < g_gridWidth && gridPos.y >= 0 && gridPos.y < g_gridHeight && g_maze[gridPos.y][gridPos.x] == PATH) {
            g_playerPosZ = newZ;
        }

        float angleRad = std::atan2(moveVector.x, moveVector.z);
        g_playerAngleY = glm::degrees(angleRad);
    }
}

void update(int value) {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - g_lastTime) / 1000.0f;
    if (deltaTime < 0.001f) deltaTime = 0.001f;
    g_lastTime = currentTime;

    handlePlayerInput(deltaTime);

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void keyboard(unsigned char key, int x, int y) {
    g_keyStates[key] = true;

    switch (key) {
    case 'q': case 'Q':
    case 27:
        glutLeaveMainLoop();
        break;
    case 'c': case 'C':
        reset();
        break;
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    g_keyStates[key] = false;
    if (key >= 'a' && key <= 'z') {
        g_keyStates[key - 32] = false;
    }
    else if (key >= 'A' && key <= 'Z') {
        g_keyStates[key + 32] = false;
    }
}

void specialKey(int key, int x, int y) {
    if (key >= 0 && key < 128) {
        g_specialKeyStates[key] = true;
    }
}

void specialKeyUp(int key, int x, int y) {
    if (key >= 0 && key < 128) {
        g_specialKeyStates[key] = false;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(g_windowWidth, g_windowHeight);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutCreateWindow("FreeGLUT Maze Project");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKey);
    glutSpecialUpFunc(specialKeyUp);
    glutTimerFunc(16, update, 0);

    init();
    glutMainLoop();

    glDeleteVertexArrays(1, &g_cubeVAO);
    glDeleteBuffers(1, &g_cubeVBO);
    glDeleteBuffers(1, &g_cubeEBO);
    glDeleteProgram(g_shaderProgram);
    return 0;
}