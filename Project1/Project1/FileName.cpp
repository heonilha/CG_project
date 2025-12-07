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
int g_gridWidth = 10;
int g_gridHeight = 10;
const float CUBE_SIZE = 0.8f;
const float GRID_SPACING = 0.2f;

GLuint g_shaderProgram = 0;
GLuint g_cubeVAO = 0, g_cubeVBO = 0, g_cubeEBO = 0;
GLint g_modelLoc = -1, g_viewLoc = -1, g_projLoc = -1, g_colorLoc = -1;

glm::vec3 g_cameraPos = glm::vec3(0.0f, 10.0f, 15.0f);
glm::vec3 g_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 g_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float g_cameraAngleY = 0.0f;
float g_cameraDistance = 15.0f;
const float DEFAULT_CAM_ANGLE = 0.0f;
const float DEFAULT_CAM_DIST = 15.0f;
bool g_isPerspective = true;

bool g_isPlayerView = false;
bool g_isFirstPerson = false;
float g_playerPosX = 0.0f;
float g_playerPosZ = 0.0f;
float g_playerAngleY = 0.0f;
int g_mazeStartX = 0;
int g_mazeEndX = 0;

const float PLAYER_WIDTH = 0.3f;
const float PLAYER_HEIGHT = 0.5f;
const float PLAYER_DEPTH = 0.3f;
const float PLAYER_MOVE_SPEED = 2.0f;
const float PLAYER_TURN_SPEED = 100.0f;

bool g_keyStates[256];
bool g_specialKeyStates[128];

bool g_isInitialAnimating = true;
bool g_isOscillating = false;
float g_animationSpeed = 1.0f;
const float DEFAULT_SPEED = 1.0f;

const float MIN_HEIGHT = -5.0f;
const float GRID_BASE_SCALE = 1.0f;
const float HIDDEN_SCALE = 0.01f;

std::vector<std::vector<float>> g_cubeRandomSpeedFactors;
std::vector<std::vector<float>> g_cubeRandomMaxScales;
std::vector<std::vector<int>> g_cubeDirections;
std::vector<std::vector<float>> g_cubeOscillationScale;

std::vector<std::vector<float>> g_cubeCurrentHeight;
std::vector<std::vector<float>> g_cubeCurrentScale;

enum CellType { WALL, PATH };
std::vector<std::vector<CellType>> g_maze;
bool g_mazeGenerated = false;
bool g_wallsHidden = false;

std::mt19937 g_randomEngine;
int g_lastTime = 0;

std::string readShaderSource(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "셰이더 파일을 열 수 없습니다: " << filePath << std::endl;
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
        std::cerr << "셰이더 컴파일 오류: " << infoLog << std::endl;
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
        std::cerr << "프로그램 링크 오류: " << infoLog << std::endl;
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
    g_cubeRandomSpeedFactors.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_cubeRandomMaxScales.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_cubeDirections.resize(g_gridHeight, std::vector<int>(g_gridWidth));
    g_cubeOscillationScale.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_cubeCurrentHeight.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_cubeCurrentScale.resize(g_gridHeight, std::vector<float>(g_gridWidth));
    g_randomEngine.seed(static_cast<unsigned int>(std::time(0)));
}

void reset() {
    g_cameraPos = glm::vec3(0.0f, 10.0f, 15.0f);
    g_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    g_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    g_cameraAngleY = DEFAULT_CAM_ANGLE;
    g_cameraDistance = DEFAULT_CAM_DIST;

    g_isPerspective = true;
    g_isInitialAnimating = true;
    g_isOscillating = false;
    g_wallsHidden = false;
    g_mazeGenerated = false;
    g_isPlayerView = false;
    g_isFirstPerson = false;
    g_playerAngleY = 0.0f;
    g_animationSpeed = DEFAULT_SPEED;

    for (int i = 0; i < 256; i++) g_keyStates[i] = false;
    for (int i = 0; i < 128; i++) g_specialKeyStates[i] = false;

    initCubes();

    std::uniform_real_distribution<float> speedDist(0.7f, 1.5f);
    std::uniform_real_distribution<float> maxScaleDist(2.5f, 5.0f);

    for (int i = 0; i < g_gridHeight; ++i) {
        for (int j = 0; j < g_gridWidth; ++j) {
            g_cubeCurrentScale[i][j] = GRID_BASE_SCALE;
            g_cubeCurrentHeight[i][j] = MIN_HEIGHT;
            g_cubeRandomSpeedFactors[i][j] = speedDist(g_randomEngine);
            g_cubeRandomMaxScales[i][j] = maxScaleDist(g_randomEngine);
            g_cubeDirections[i][j] = 1;
            g_cubeOscillationScale[i][j] = 0.0f;
        }
    }
}

void init() {
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {}

    std::string vsCode = readShaderSource("vertex.glsl");
    std::string fsCode = readShaderSource("fragment.glsl");

    if (vsCode.empty() || fsCode.empty()) {
        std::cerr << "셰이더 소스가 비어 있습니다. 파일 경로를 확인하세요." << std::endl;
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

void updateCamera() {
    float camX = g_cameraDistance * std::sin(glm::radians(g_cameraAngleY));
    float camZ = g_cameraDistance * std::cos(glm::radians(g_cameraAngleY));
    g_cameraPos = glm::vec3(camX, g_cameraPos.y, camZ);
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

    if (g_isPlayerView && !g_isFirstPerson) {
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
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, g_windowWidth, g_windowHeight);
    glm::mat4 view(1.0f);

    if (g_isPlayerView) {
        glm::ivec2 gridPos = getGridCoord(g_playerPosX, g_playerPosZ);
        float tileY = 0.0f;
        if (gridPos.y >= 0 && gridPos.y < g_gridHeight && gridPos.x >= 0 && gridPos.x < g_gridWidth) {
            tileY = g_cubeCurrentHeight[gridPos.y][gridPos.x];
        }
        glm::vec3 playerWorldPos = glm::vec3(g_playerPosX, tileY, g_playerPosZ);
        float angleRad = glm::radians(g_playerAngleY);

        if (g_isFirstPerson) {
            float floorOffset = (GRID_BASE_SCALE * CUBE_SIZE / 2.0f);
            float eyeLevelFromFloor = PLAYER_HEIGHT * 0.4f;

            g_cameraPos = glm::vec3(g_playerPosX, tileY + floorOffset + eyeLevelFromFloor, g_playerPosZ);

            glm::vec3 forward(sin(angleRad), 0.0f, cos(angleRad));
            g_cameraTarget = g_cameraPos + forward;
        }
        else {
            float camX = playerWorldPos.x - glm::sin(angleRad) * 6.0f;
            float camZ = playerWorldPos.z - glm::cos(angleRad) * 6.0f;
            float camY = playerWorldPos.y + 5.0f;
            g_cameraPos = glm::vec3(camX, camY, camZ);
            g_cameraTarget = playerWorldPos + glm::vec3(0.0f, 1.0f, 0.0f);
        }

        view = glm::lookAt(g_cameraPos, g_cameraTarget, g_cameraUp);
    }
    else {
        updateCamera();
        view = glm::lookAt(g_cameraPos, g_cameraTarget, g_cameraUp);
    }

    glm::mat4 projection(1.0f);
    float aspect = (float)g_windowWidth / g_windowHeight;
    if (g_isPerspective) {
        projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
    else {
        float orthoSize = g_cameraDistance * 0.5f;
        projection = glm::ortho(-orthoSize * aspect, orthoSize * aspect, -orthoSize, orthoSize, -100.0f, 100.0f);
    }

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
    if (g_isInitialAnimating) return;

    float turnDelta = PLAYER_TURN_SPEED * deltaTime;
    if (g_isPlayerView) {
        if (g_keyStates['y']) g_playerAngleY += turnDelta;
        if (g_keyStates['Y']) g_playerAngleY -= turnDelta;
    }
    else {
        if (g_keyStates['y']) g_cameraAngleY += 3.0f;
        if (g_keyStates['Y']) g_cameraAngleY -= 3.0f;
    }
    if (g_keyStates['z']) g_cameraDistance += 0.1f;
    if (g_keyStates['Z']) g_cameraDistance -= 0.1f;

    if (!g_isPlayerView) return;

    float angleRad = glm::radians(g_playerAngleY);
    glm::vec3 forward(glm::sin(angleRad), 0, glm::cos(angleRad));
    glm::vec3 right(forward.z, 0, -forward.x);

    glm::vec3 moveVector(0.0f, 0.0f, 0.0f);

    if (g_specialKeyStates[GLUT_KEY_UP])    moveVector += forward;
    if (g_specialKeyStates[GLUT_KEY_DOWN])  moveVector -= forward;
    if (g_specialKeyStates[GLUT_KEY_LEFT])  moveVector += right;
    if (g_specialKeyStates[GLUT_KEY_RIGHT]) moveVector -= right;

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
    }
}

void update(int value) {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - g_lastTime) / 1000.0f;
    if (deltaTime < 0.001f) deltaTime = 0.001f;
    g_lastTime = currentTime;
    float currentSpeed = g_animationSpeed * deltaTime;

    handlePlayerInput(deltaTime);

    if (g_isInitialAnimating) {
        bool allCubesAtTarget = true;
        float targetScale = GRID_BASE_SCALE;
        float targetHeight = (targetScale * CUBE_SIZE) / 2.0f;
        for (int i = 0; i < g_gridHeight; ++i) {
            for (int j = 0; j < g_gridWidth; ++j) {
                if (g_cubeCurrentHeight[i][j] < targetHeight) {
                    g_cubeCurrentHeight[i][j] += currentSpeed * 2.0f;
                    if (g_cubeCurrentHeight[i][j] >= targetHeight) {
                        g_cubeCurrentHeight[i][j] = targetHeight;
                    }
                    else {
                        allCubesAtTarget = false;
                    }
                }
                g_cubeCurrentScale[i][j] = targetScale;
            }
        }
        if (allCubesAtTarget) {
            g_isInitialAnimating = false;
            std::cout << "애니메이션 완료" << std::endl;
        }
    }
    else {
        for (int i = 0; i < g_gridHeight; ++i) {
            for (int j = 0; j < g_gridWidth; ++j) {
                float finalScale = GRID_BASE_SCALE;
                if (g_isOscillating) {
                    float speedFactor = g_cubeRandomSpeedFactors[i][j];
                    float maxOscScale = g_cubeRandomMaxScales[i][j];
                    float cubeSpeed = currentSpeed * speedFactor;
                    g_cubeOscillationScale[i][j] += g_cubeDirections[i][j] * cubeSpeed;

                    float oscScale = g_cubeOscillationScale[i][j];
                    if (oscScale > maxOscScale) {
                        oscScale = maxOscScale; g_cubeDirections[i][j] = -1;
                    }
                    else if (oscScale < 0.0f) {
                        oscScale = 0.0f; g_cubeDirections[i][j] = 1;
                    }
                    g_cubeOscillationScale[i][j] = oscScale;
                }
                finalScale += g_cubeOscillationScale[i][j];

                if (g_wallsHidden && g_maze[i][j] == PATH) {
                    finalScale = HIDDEN_SCALE;
                }
                g_cubeCurrentScale[i][j] = finalScale;
                g_cubeCurrentHeight[i][j] = (finalScale * CUBE_SIZE) / 2.0f;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void keyboard(unsigned char key, int x, int y) {
    g_keyStates[key] = true;

    switch (key) {
    case 'q': case 'Q':
        glutLeaveMainLoop();
        break;
    case 'c': case 'C':
        std::cout << "모든 설정 초기화" << std::endl;
        reset();
        break;
    case 'r': case 'R':
        if (g_isInitialAnimating) break;
        if (g_wallsHidden) {
            std::cout << "이미 미로가 생성되어 있습니다." << std::endl;
        }
        else {
            g_wallsHidden = true;
            std::cout << "미로 생성 및 벽 숨기기: ON" << std::endl;

            if (!g_mazeGenerated) {
                std::cout << "미로 알고리즘 실행 중..." << std::endl;
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
                g_mazeGenerated = true;
            }
        }
        break;

    case 'm':
        if (g_isInitialAnimating) break;
        g_isOscillating = true;
        std::cout << "진동 시작" << std::endl;
        break;
    case 'M':
        if (g_isInitialAnimating) break;
        g_isOscillating = false;
        std::cout << "진동 일시 정지" << std::endl;
        break;

    case 'v': case 'V':
        if (g_isInitialAnimating) break;
        g_isOscillating = false;
        std::cout << "진동 초기화 (높이 복귀)" << std::endl;
        for (int i = 0; i < g_gridHeight; ++i) {
            for (int j = 0; j < g_gridWidth; ++j) {
                g_cubeOscillationScale[i][j] = 0.0f;

                float finalScale = GRID_BASE_SCALE;
                if (g_wallsHidden && g_maze[i][j] == WALL) finalScale = HIDDEN_SCALE;
                g_cubeCurrentScale[i][j] = finalScale;
                g_cubeCurrentHeight[i][j] = (finalScale * CUBE_SIZE) / 2.0f;
            }
        }
        break;

    case 'o': case 'O':
        g_isPerspective = false;
        std::cout << "투영 방식: 직각 투영" << std::endl;
        break;
    case 'p': case 'P':
        g_isPerspective = true;
        std::cout << "투영 방식: 원근 투영" << std::endl;
        break;

    case 's': case 'S':
        if (g_isInitialAnimating) break;
        if (g_mazeGenerated) {
            if (!g_isPlayerView) {
                g_isPlayerView = true;
                std::cout << "플레이어 모드 시작" << std::endl;

                glm::vec3 startPos = getWorldPos(g_mazeEndX, g_gridHeight - 1);
                g_playerPosX = startPos.x;
                g_playerPosZ = startPos.z;
                g_playerAngleY = 180.0f;
                g_isFirstPerson = false;
            }
            else {
                std::cout << "이미 플레이어 모드입니다." << std::endl;
            }
        }
        else {
            std::cout << "먼저 미로를 생성해야 합니다 ('r' 키)" << std::endl;
        }
        break;

    case '1':
        if (g_isPlayerView) {
            g_isFirstPerson = true;
            std::cout << "시점: 1인칭" << std::endl;
        }
        break;
    case '3':
        if (g_isPlayerView) {
            g_isFirstPerson = false;
            std::cout << "시점: 3인칭" << std::endl;
        }
        break;

    case 'y': case 'Y':
        std::cout << "회전 (Y)" << std::endl;
        break;
    case 'z': case 'Z':
        std::cout << "줌 (Z)" << std::endl;
        break;

    case '+':
        g_animationSpeed += 0.2f;
        std::cout << "속도: " << g_animationSpeed << std::endl;
        break;
    case '-':
        g_animationSpeed -= 0.2f;
        if (g_animationSpeed < 0.2f) g_animationSpeed = 0.2f;
        std::cout << "속도: " << g_animationSpeed << std::endl;
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
        switch (key) {
        case GLUT_KEY_UP: std::cout << "이동: 앞" << std::endl; break;
        case GLUT_KEY_DOWN: std::cout << "이동: 뒤" << std::endl; break;
        case GLUT_KEY_LEFT: std::cout << "이동: 좌" << std::endl; break;
        case GLUT_KEY_RIGHT: std::cout << "이동: 우" << std::endl; break;
        }
    }
}

void specialKeyUp(int key, int x, int y) {
    if (key >= 0 && key < 128) {
        g_specialKeyStates[key] = false;
    }
}

int main(int argc, char** argv) {
    int width = 0, height = 0;
    const int MIN_GRID = 5;
    const int MAX_GRID = 25;
    while (width < MIN_GRID || width > MAX_GRID) {
        std::cout << "가로 개수 (" << MIN_GRID << "~" << MAX_GRID << "): ";
        std::cin >> width;
    }
    g_gridWidth = width;
    while (height < MIN_GRID || height > MAX_GRID) {
        std::cout << "세로 개수 (" << MIN_GRID << "~" << MAX_GRID << "): ";
        std::cin >> height;
    }
    g_gridHeight = height;
    std::cout << g_gridWidth << " x " << g_gridHeight << " 그리드 생성 완료" << std::endl;

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