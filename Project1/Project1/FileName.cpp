#include <GL/glew.h>
#include <GL/glu.h>
#include <gl/freeglut.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>
#include <gl/glm/gtc/type_ptr.hpp>

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
#include <limits>

int g_windowWidth = 1024;
int g_windowHeight = 768;
int g_gridWidth = 11;
int g_gridHeight = 11;
const float CUBE_SIZE = 0.8f;
const float GRID_SPACING = 0.2f;

GLuint g_shaderProgram = 0;
GLuint g_cubeVAO = 0, g_cubeVBO = 0, g_cubeEBO = 0;
GLuint g_sphereVAO = 0, g_sphereVBO = 0, g_sphereEBO = 0;
GLsizei g_sphereIndexCount = 0;

GLuint g_cylinderVAO = 0, g_cylinderVBO = 0, g_cylinderEBO = 0;
GLsizei g_cylinderIndexCount = 0;
GLint g_modelLoc = -1, g_viewLoc = -1, g_projLoc = -1, g_colorLoc = -1, g_clipSignLoc = -1, g_lightPosLoc = -1, g_lightDirLoc = -1, g_cutOffLoc = -1, g_outerCutOffLoc = -1;

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
const float PLAYER_MOVE_SPEED = 4.0f;
float g_pacmanMouthAngle = 55.0f;          // 현재 입 각도(도)
float g_pacmanMouthDir = 1.0f;             // 1 = 열리는 중, -1 = 닫히는 중

const float PACMAN_MOUTH_MAX = 55.0f;      // 최대 입 벌림 각도 (더 크게 벌리기)
const float PACMAN_MOUTH_SPEED = 120.0f;   // 1초에 120도 정도 회전
bool g_keyStates[256];
bool g_specialKeyStates[128];
const float GRID_BASE_SCALE = 1.0f;
const float WALL_SCALE = 2.0f;
const float FLOOR_SCALE = 0.05f;
bool g_isMinimapView = false;

struct Ghost {
    float x;
    float z;
    float angleY;
    float speed;
    int dirX;
    int dirZ;
};

std::vector<Ghost> g_ghosts;

const float GHOST_WIDTH = 0.3f;
const float GHOST_HEIGHT = 0.5f;
const float GHOST_DEPTH = 0.3f;
const float GHOST_MOVE_SPEED = 2.0f;  // 플레이어보다 느리게 이동


std::vector<std::vector<float>> g_cubeCurrentHeight;
std::vector<std::vector<float>> g_cubeCurrentScale;
std::vector<std::vector<bool>> g_pellets;      // 해당 칸에 펠릿이 있는지 여부
std::vector<std::vector<bool>> g_slowItems;    // Stage 2에서만 등장하는 특수 아이템
int g_totalPellets = 0;                        // 맵 전체 펠릿 수
int g_remainingPellets = 0;                    // 아직 안 먹은 펠릿 수

bool  g_ghostSlowActive = false;
float g_ghostSlowTimer = 0.0f;
float g_ghostSpeedScale = 1.0f;      // 1.0 = 기본, 0.5 = 절반 속도 등

const float GHOST_SLOW_DURATION = 5.0f;   // 5초 동안 지속 (나중에 조절 가능)
const float GHOST_SLOW_SCALE    = 0.5f;   // 유령 속도 50%로 감소

enum CellType { WALL, PATH };
std::vector<std::vector<CellType>> g_maze;

std::mt19937 g_randomEngine;
int g_lastTime = 0;

enum class GameState {
    TITLE,
    PLAYING,
    GAME_CLEAR,
    GAME_OVER
};

GameState g_gameState = GameState::TITLE;

int g_score = 0;
int g_lives = 3;
int g_currentStage = 1;   // 1 = Stage 1, 2 = Stage 2
const int MAX_STAGE = 2;


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

void addMazeLoops(float loopProbability)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int z = 1; z < g_gridHeight - 1; ++z) {
        for (int x = 1; x < g_gridWidth - 1; ++x) {
            if (g_maze[z][x] != WALL) continue;

            bool horiz = (g_maze[z][x - 1] == PATH && g_maze[z][x + 1] == PATH);
            bool vert  = (g_maze[z - 1][x] == PATH && g_maze[z + 1][x] == PATH);

            if ((horiz || vert) && dist(g_randomEngine) < loopProbability) {
                g_maze[z][x] = PATH;
            }
        }
    }
}

void initCubes() {
    // `resize` would keep the width of existing rows, so when the stage size grows
    // (e.g., moving from 11x11 to 25x25) previously allocated rows remain too short
    // and later indexing with the new width crashes. `assign` rebuilds each row with
    // the correct column count for the current stage.
    g_maze.assign(g_gridHeight, std::vector<CellType>(g_gridWidth, WALL));
    g_cubeCurrentHeight.assign(g_gridHeight, std::vector<float>(g_gridWidth, 0.0f));
    g_cubeCurrentScale.assign(g_gridHeight, std::vector<float>(g_gridWidth, 0.0f));
    g_pellets.assign(g_gridHeight, std::vector<bool>(g_gridWidth, false));
    g_slowItems.assign(g_gridHeight, std::vector<bool>(g_gridWidth, false));
    g_randomEngine.seed(static_cast<unsigned int>(std::time(0)));
}

void reset() {
    int stageGridWidth = 11;
    int stageGridHeight = 11;
    float loopProbability = 0.35f;
    int ghostCount = 3;

    g_ghostSlowActive = false;
    g_ghostSlowTimer = 0.0f;
    g_ghostSpeedScale = 1.0f;

    if (g_currentStage == 2) {
        stageGridWidth = 25;
        stageGridHeight = 25;
        loopProbability = 0.5f;
        ghostCount = 7;
    }

    g_gridWidth = stageGridWidth;
    g_gridHeight = stageGridHeight;

    g_cameraPos = glm::vec3(0.0f, 10.0f, 15.0f);
    g_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    g_cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    g_playerAngleY = 0.0f;

    for (int i = 0; i < 256; i++) g_keyStates[i] = false;
    for (int i = 0; i < 128; i++) g_specialKeyStates[i] = false;

    initCubes();

    g_maze.assign(g_gridHeight, std::vector<CellType>(g_gridWidth, WALL));
    g_slowItems.assign(g_gridHeight, std::vector<bool>(g_gridWidth, false));
    g_totalPellets = 0;
    g_remainingPellets = 0;
    int range = (g_gridWidth - 3) / 2;
    if (range < 0) range = 0;
    std::uniform_int_distribution<int> xDist(0, range);
    g_mazeStartX = xDist(g_randomEngine) * 2 + 1;
    g_mazeEndX = xDist(g_randomEngine) * 2 + 1;
    generateMaze(g_mazeEndX, g_gridHeight - 2);
    g_maze[0][g_mazeStartX] = PATH;
    g_maze[1][g_mazeStartX] = PATH;
    g_maze[g_gridHeight - 1][g_mazeEndX] = PATH;

    addMazeLoops(loopProbability);

    glm::vec3 playerStartPos = getWorldPos(g_mazeStartX, 0);
    g_playerPosX = playerStartPos.x;
    g_playerPosZ = playerStartPos.z;

    g_ghosts.clear();

    auto findNearestPath = [&](int gridX, int gridZ) {
        int maxRadius = 3;
        for (int radius = 0; radius <= maxRadius; ++radius) {
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = gridX + dx;
                    int nz = gridZ + dz;
                    if (nx < 0 || nx >= g_gridWidth || nz < 0 || nz >= g_gridHeight) continue;
                    if (g_maze[nz][nx] == PATH) {
                        return glm::ivec2(nx, nz);
                    }
                }
            }
        }

        return glm::ivec2(g_mazeStartX, 0);
    };

    auto addGhostAt = [&](int gridX, int gridZ, int dirX, int dirZ) {
        glm::ivec2 pathCell = findNearestPath(gridX, gridZ);
        glm::vec3 worldPos = getWorldPos(pathCell.x, pathCell.y);
        Ghost ghost;
        ghost.x = worldPos.x;
        ghost.z = worldPos.z;
        ghost.angleY = 0.0f;
        ghost.speed = GHOST_MOVE_SPEED;
        ghost.dirX = dirX;
        ghost.dirZ = dirZ;
        g_ghosts.push_back(ghost);
    };

    std::uniform_int_distribution<int> ghostXDist(1, g_gridWidth - 2);
    std::uniform_int_distribution<int> ghostZDist(1, g_gridHeight - 2);
    const int dirChoices[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    for (int i = 0; i < ghostCount; ++i) {
        int dirIndex = i % 4;
        int dirX = dirChoices[dirIndex][0];
        int dirZ = dirChoices[dirIndex][1];
        addGhostAt(ghostXDist(g_randomEngine), ghostZDist(g_randomEngine), dirX, dirZ);
    }

    for (int i = 0; i < g_gridHeight; ++i) {
        for (int j = 0; j < g_gridWidth; ++j) {
            if (g_maze[i][j] == WALL) {
                g_cubeCurrentScale[i][j] = WALL_SCALE;
                g_pellets[i][j] = false;
            }
            else {
                g_cubeCurrentScale[i][j] = FLOOR_SCALE;
                g_pellets[i][j] = true;
                g_totalPellets++;
                g_remainingPellets++;
            }
            g_cubeCurrentHeight[i][j] = (g_cubeCurrentScale[i][j] * CUBE_SIZE) / 2.0f;
        }
    }

    if (g_currentStage == 2) {
        std::vector<std::pair<int, int>> pathCells;
        for (int i = 0; i < g_gridHeight; ++i) {
            for (int j = 0; j < g_gridWidth; ++j) {
                if (g_maze[i][j] == PATH) {
                    pathCells.emplace_back(j, i);
                }
            }
        }

        if (!pathCells.empty()) {
            std::shuffle(pathCells.begin(), pathCells.end(), g_randomEngine);
            std::uniform_int_distribution<int> slowItemDist(3, 5);
            int slowItemCount = std::min(static_cast<int>(pathCells.size()), slowItemDist(g_randomEngine));

            for (int idx = 0; idx < slowItemCount; ++idx) {
                int x = pathCells[idx].first;
                int y = pathCells[idx].second;
                g_slowItems[y][x] = true;
                if (g_pellets[y][x]) {
                    g_pellets[y][x] = false;
                    g_totalPellets--;
                    g_remainingPellets--;
                }
            }
        }
    }
}

void startNewGame() {
    g_currentStage = 1;
    g_score = 0;
    g_lives = 3;
    reset();
    g_gameState = GameState::PLAYING;
}

void goToTitle() {
    g_gameState = GameState::TITLE;
}

void goToGameOver() {
    g_gameState = GameState::GAME_OVER;
}

void goToGameClear() {
    if (g_currentStage < MAX_STAGE) {
        g_gameState = GameState::GAME_CLEAR;
    }
    else {
        g_gameState = GameState::GAME_CLEAR;
    }
}

void initSphereMesh(int sectorCount, int stackCount) {
    const float radius = 0.5f;
    const float PI = 3.14159265358979323846f;

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2.0f - i * (PI / stackCount);
        float xy = radius * cosf(stackAngle);
        float y = radius * sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * (2 * PI / sectorCount);
            float x = xy * cosf(sectorAngle);
            float z = xy * sinf(sectorAngle);
            glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;

        for (int j = 0; j < sectorCount; ++j) {
            if (i != 0) {
                indices.push_back(k1 + j);
                indices.push_back(k2 + j);
                indices.push_back(k1 + j + 1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1 + j + 1);
                indices.push_back(k2 + j);
                indices.push_back(k2 + j + 1);
            }
        }
    }

    g_sphereIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &g_sphereVAO);
    glGenBuffers(1, &g_sphereVBO);
    glGenBuffers(1, &g_sphereEBO);

    glBindVertexArray(g_sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void initCylinderMesh(int sectorCount) {
    const float radius = 0.6f;
    const float height = 2.0f;
    const float halfHeight = height / 2.0f;
    const float PI = 3.14159265358979323846f;

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    vertices.insert(vertices.end(), { 0.0f, halfHeight, 0.0f, 0.0f, 1.0f, 0.0f });   // top center
    vertices.insert(vertices.end(), { 0.0f, -halfHeight, 0.0f, 0.0f, -1.0f, 0.0f }); // bottom center

    float sectorStep = 2 * PI / sectorCount;
    for (int i = 0; i <= sectorCount; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        vertices.push_back(x); vertices.push_back(halfHeight); vertices.push_back(z);
        vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
    }

    for (int i = 0; i <= sectorCount; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        vertices.push_back(x); vertices.push_back(-halfHeight); vertices.push_back(z);
        vertices.push_back(0.0f); vertices.push_back(-1.0f); vertices.push_back(0.0f);
    }

    for (int i = 0; i <= sectorCount; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        float nx = cosf(angle);
        float nz = sinf(angle);
        vertices.push_back(x); vertices.push_back(halfHeight); vertices.push_back(z);
        vertices.push_back(nx); vertices.push_back(0.0f); vertices.push_back(nz);
    }

    for (int i = 0; i <= sectorCount; ++i) {
        float angle = i * sectorStep;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        float nx = cosf(angle);
        float nz = sinf(angle);
        vertices.push_back(x); vertices.push_back(-halfHeight); vertices.push_back(z);
        vertices.push_back(nx); vertices.push_back(0.0f); vertices.push_back(nz);
    }

    GLuint topCenter = 0;
    GLuint bottomCenter = 1;
    GLuint topStart = 2;
    GLuint bottomStart = topStart + sectorCount + 1;
    GLuint sideTopStart = bottomStart + sectorCount + 1;
    GLuint sideBottomStart = sideTopStart + sectorCount + 1;

    for (int i = 0; i < sectorCount; ++i) {
        indices.push_back(topCenter);
        indices.push_back(topStart + i);
        indices.push_back(topStart + i + 1);

        indices.push_back(bottomCenter);
        indices.push_back(bottomStart + i + 1);
        indices.push_back(bottomStart + i);

        GLuint k1 = sideTopStart + i;
        GLuint k2 = sideBottomStart + i;

        indices.push_back(k1);
        indices.push_back(k2);
        indices.push_back(k1 + 1);

        indices.push_back(k1 + 1);
        indices.push_back(k2);
        indices.push_back(k2 + 1);
    }

    g_cylinderIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &g_cylinderVAO);
    glGenBuffers(1, &g_cylinderVBO);
    glGenBuffers(1, &g_cylinderEBO);

    glBindVertexArray(g_cylinderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_cylinderVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_cylinderEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void drawSphere()
{
    glBindVertexArray(g_sphereVAO);
    glDrawElements(GL_TRIANGLES, g_sphereIndexCount, GL_UNSIGNED_INT, (void*)0);
}

void drawCylinder()
{
    glBindVertexArray(g_cylinderVAO);
    glDrawElements(GL_TRIANGLES, g_cylinderIndexCount, GL_UNSIGNED_INT, (void*)0);
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
    g_lightPosLoc = glGetUniformLocation(g_shaderProgram, "lightPos");
    g_lightDirLoc = glGetUniformLocation(g_shaderProgram, "lightDir");
    g_cutOffLoc = glGetUniformLocation(g_shaderProgram, "cutOff");
    g_outerCutOffLoc = glGetUniformLocation(g_shaderProgram, "outerCutOff");
    g_clipSignLoc = glGetUniformLocation(g_shaderProgram, "clipSign");

    float s = 0.5f;
    GLfloat vertices[] = {
        // Front face
        -s, -s,  s,  0.0f, 0.0f, 1.0f,
         s, -s,  s,  0.0f, 0.0f, 1.0f,
         s,  s,  s,  0.0f, 0.0f, 1.0f,
        -s,  s,  s,  0.0f, 0.0f, 1.0f,
        // Back face
        -s, -s, -s,  0.0f, 0.0f, -1.0f,
        -s,  s, -s,  0.0f, 0.0f, -1.0f,
         s,  s, -s,  0.0f, 0.0f, -1.0f,
         s, -s, -s,  0.0f, 0.0f, -1.0f,
        // Left face
        -s, -s, -s, -1.0f, 0.0f, 0.0f,
        -s, -s,  s, -1.0f, 0.0f, 0.0f,
        -s,  s,  s, -1.0f, 0.0f, 0.0f,
        -s,  s, -s, -1.0f, 0.0f, 0.0f,
        // Right face
         s, -s, -s, 1.0f, 0.0f, 0.0f,
         s,  s, -s, 1.0f, 0.0f, 0.0f,
         s,  s,  s, 1.0f, 0.0f, 0.0f,
         s, -s,  s, 1.0f, 0.0f, 0.0f,
        // Bottom face
        -s, -s, -s, 0.0f, -1.0f, 0.0f,
         s, -s, -s, 0.0f, -1.0f, 0.0f,
         s, -s,  s, 0.0f, -1.0f, 0.0f,
        -s, -s,  s, 0.0f, -1.0f, 0.0f,
        // Top face
        -s,  s, -s, 0.0f, 1.0f, 0.0f,
        -s,  s,  s, 0.0f, 1.0f, 0.0f,
         s,  s,  s, 0.0f, 1.0f, 0.0f,
         s,  s, -s, 0.0f, 1.0f, 0.0f
    };

    GLuint indices[] = {
        20, 21, 22, 22, 23, 20, // top
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16
    };
    glGenVertexArrays(1, &g_cubeVAO); glGenBuffers(1, &g_cubeVBO); glGenBuffers(1, &g_cubeEBO);
    glBindVertexArray(g_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    initSphereMesh(24, 16);
    initCylinderMesh(24);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    g_lastTime = glutGet(GLUT_ELAPSED_TIME);
    reset();
}

void renderText(float x, float y, const std::string& text)
{
    // --- Modern OpenGL 상태 차단 ---
    glUseProgram(0);          // 셰이더 OFF
    glBindVertexArray(0);     // VAO OFF (이거 안 하면 텍스트 안 보임!!)
    glDisable(GL_DEPTH_TEST); // 깊이 테스트 OFF

    // --- 색상 ---
    glColor3f(1.0f, 1.0f, 1.0f);

    // --- 화면 좌표 직접 지정 ---
    // glRasterPos는 클리핑되면 작동 안 하므로, glWindowPos로 직접 찍음
    glWindowPos2f(x, y);

    // --- 글자 출력 ---
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    // --- 상태 복구 ---
    glEnable(GL_DEPTH_TEST);
}


void drawCube() {
    glBindVertexArray(g_cubeVAO);

    if (g_isMinimapView) {
        // 미니맵에서는 바깥에서 지정한 색을 그대로 사용
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)(0));
    }
    else {
        // 메인 3D 화면용: 윗면/옆면 색 다르게
        glUniform3f(g_colorLoc, 0.0f, 0.0f, 1.0f); // 윗면
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(0));
        glUniform3f(g_colorLoc, 0.0f, 0.0f, 0.0f); // 나머지
        glDrawElements(GL_TRIANGLES, 30, GL_UNSIGNED_INT, (void*)(6 * sizeof(GLuint)));
    }
}

void drawHemisphere(float clipSign) {
    // clipSign = +1 : y >= 0만 남김 (위쪽 반구)
    // clipSign = -1 : y <= 0만 남김 (아래쪽 반구)
    glEnable(GL_CLIP_DISTANCE0);
    glUniform1f(g_clipSignLoc, clipSign);

    glBindVertexArray(g_sphereVAO);
    glDrawElements(GL_TRIANGLES, g_sphereIndexCount, GL_UNSIGNED_INT, (void*)0);

    glDisable(GL_CLIP_DISTANCE0);
}

void drawPacman(const glm::vec3& worldPos) {
    // 팩맨 전체 스케일 (반구가 정확한 구 형태가 되도록 동일 반지름 사용)
    float radius = PLAYER_HEIGHT; // 높이와 동일한 반지름
    float radiusX = radius;
    float radiusY = radius;
    float radiusZ = radius;

    // 공통 회전 (플레이어 방향)
    glm::mat4 baseRot = glm::rotate(glm::mat4(1.0f),
        glm::radians(g_playerAngleY),
        glm::vec3(0.0f, 1.0f, 0.0f));

    // 위 턱(반구)
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, worldPos);

        // 팩맨 전체 회전 + 입 회전 (위쪽으로 열림)
        model *= baseRot;
        model = glm::rotate(model,
            glm::radians(+g_pacmanMouthAngle),
            glm::vec3(1.0f, 0.0f, 0.0f)); // X축 기준으로 회전

        model = glm::scale(model, glm::vec3(radiusX, radiusY, radiusZ));

        glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(g_colorLoc, 1.0f, 1.0f, 0.0f);  // 노란 팩맨
        drawHemisphere(+1.0f);
    }

    // 아래 턱(반구)
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, worldPos);

        model *= baseRot;
        model = glm::rotate(model,
            glm::radians(-g_pacmanMouthAngle),
            glm::vec3(1.0f, 0.0f, 0.0f));

        model = glm::scale(model, glm::vec3(radiusX, radiusY, radiusZ));

        glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(g_colorLoc, 1.0f, 1.0f, 0.0f);
        drawHemisphere(-1.0f);
    }
}

void drawGhost(const Ghost& ghost) {
    glm::ivec2 gGrid = getGridCoord(ghost.x, ghost.z);
    float gTileY = 0.0f;
    float gTileScale = FLOOR_SCALE;
    if (gGrid.y >= 0 && gGrid.y < g_gridHeight &&
        gGrid.x >= 0 && gGrid.x < g_gridWidth) {
        gTileY = g_cubeCurrentHeight[gGrid.y][gGrid.x];
        gTileScale = g_cubeCurrentScale[gGrid.y][gGrid.x];
    }

    float baseY = gTileY + (gTileScale * CUBE_SIZE * 0.5f);

    float bodyHeight = GHOST_HEIGHT * 0.7f;
    float headRadius = bodyHeight;

    // 1) 몸통(원기둥)
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(ghost.x, baseY + bodyHeight * 0.5f, ghost.z));
        model = glm::rotate(model, glm::radians(ghost.angleY), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(GHOST_WIDTH, bodyHeight, GHOST_DEPTH));

        glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(g_colorLoc, 0.6f, 0.6f, 0.6f);  // 회색 유령
        drawCylinder();
    }

    // 2) 머리(구)
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(ghost.x, baseY + bodyHeight + headRadius, ghost.z));
        model = glm::rotate(model, glm::radians(ghost.angleY), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(headRadius, headRadius, headRadius));

        glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(g_colorLoc, 0.6f, 0.6f, 0.6f);
        drawSphere();
    }
}


void drawGrid(glm::mat4 view, glm::mat4 projection, const glm::vec3& lightPos) {
    glUseProgram(g_shaderProgram);
    glUniformMatrix4fv(g_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(g_lightPosLoc, 1, glm::value_ptr(lightPos));
    glm::vec3 lightDirection = g_isMinimapView ? glm::vec3(0.0f, -1.0f, 0.0f) : glm::normalize(g_cameraTarget - g_cameraPos);
    glUniform3fv(g_lightDirLoc, 1, glm::value_ptr(lightDirection));
    glUniform1f(g_cutOffLoc, glm::cos(glm::radians(15.0f)));
    glUniform1f(g_outerCutOffLoc, glm::cos(glm::radians(22.5f)));
    float totalGridWidth = (g_gridWidth - 1) * (CUBE_SIZE + GRID_SPACING);
    float totalGridHeight = (g_gridHeight - 1) * (CUBE_SIZE + GRID_SPACING);
    float startX = -totalGridWidth / 2.0f;
    float startZ = -totalGridHeight / 2.0f;

    for (int i = 0; i < g_gridHeight; ++i) {
        for (int j = 0; j < g_gridWidth; ++j) {
            float x = startX + j * (CUBE_SIZE + GRID_SPACING);
            float z = startZ + i * (CUBE_SIZE + GRID_SPACING);
            float y = g_cubeCurrentHeight[i][j];
            float scaleY = g_cubeCurrentScale[i][j];

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(x, y, z));
            model = glm::scale(model, glm::vec3(CUBE_SIZE, scaleY * CUBE_SIZE, CUBE_SIZE));
            glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            // ★ 여기서 색 결정
            if (g_isMinimapView) {
                // 미니맵용 색
                if (g_maze[i][j] == WALL) {
                    // 예: 벽 = 흰색, 바닥 = 검정
                    glUniform3f(g_colorLoc, 1.0f, 1.0f, 1.0f);   // 벽 윗부분 밝게
                }
                else { // PATH
                    glUniform3f(g_colorLoc, 0.0f, 0.0f, 0.0f);   // 바닥 검정
                }
            }
            else {
                // 메인 화면용 색(취향대로)
                if (g_maze[i][j] == WALL) {
                    glUniform3f(g_colorLoc, 0.4f, 0.4f, 0.9f);   // 벽 파란 계열
                }
                else {
                    glUniform3f(g_colorLoc, 0.0f, 0.0f, 0.0f);   // 바닥 검정색
                }
            }

            drawCube();

            // --- 미니맵용 펠릿 표시 ---
            if (g_isMinimapView && g_maze[i][j] == PATH && g_pellets[i][j]) {
                glm::mat4 pelletModel = glm::mat4(1.0f);

                float pelletY = g_cubeCurrentHeight[i][j]
                    + (g_cubeCurrentScale[i][j] * CUBE_SIZE * 0.5f)
                    + 0.02f;

                pelletModel = glm::translate(pelletModel, glm::vec3(x, pelletY, z));

                float pelletScale = CUBE_SIZE * 0.2f;
                pelletModel = glm::scale(pelletModel, glm::vec3(pelletScale, pelletScale, pelletScale));

                glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(pelletModel));
                glUniform3f(g_colorLoc, 1.0f, 0.9f, 0.2f);
                drawCube();
            }

            if (g_isMinimapView && g_maze[i][j] == PATH && g_slowItems[i][j]) {
                glm::mat4 itemModel = glm::mat4(1.0f);

                float itemY = g_cubeCurrentHeight[i][j]
                    + (g_cubeCurrentScale[i][j] * CUBE_SIZE * 0.5f)
                    + 0.025f;

                itemModel = glm::translate(itemModel, glm::vec3(x, itemY, z));

                float itemScale = CUBE_SIZE * 0.22f;
                itemModel = glm::scale(itemModel, glm::vec3(itemScale, itemScale, itemScale));

                glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(itemModel));
                glUniform3f(g_colorLoc, 0.2f, 0.8f, 1.0f);
                drawCube();
            }

            // 펠릿 그리기 (메인 화면에서만)
            if (!g_isMinimapView && g_maze[i][j] == PATH && g_pellets[i][j]) {
                glm::mat4 pelletModel = glm::mat4(1.0f);

                float pelletY = g_cubeCurrentHeight[i][j] + (g_cubeCurrentScale[i][j] * CUBE_SIZE * 0.5f) + 0.05f;
                pelletModel = glm::translate(pelletModel, glm::vec3(x, pelletY, z));

                float pelletScale = 0.2f;
                pelletModel = glm::scale(pelletModel, glm::vec3(pelletScale, pelletScale, pelletScale));

                glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(pelletModel));
                glUniform3f(g_colorLoc, 1.0f, 0.9f, 0.2f);
                drawCube();
            }

            if (!g_isMinimapView && g_maze[i][j] == PATH && g_slowItems[i][j]) {
                glm::mat4 itemModel = glm::mat4(1.0f);

                float itemY = g_cubeCurrentHeight[i][j] + (g_cubeCurrentScale[i][j] * CUBE_SIZE * 0.5f) + 0.06f;
                itemModel = glm::translate(itemModel, glm::vec3(x, itemY, z));

                float itemScale = 0.25f;
                itemModel = glm::scale(itemModel, glm::vec3(itemScale, itemScale, itemScale));

                glUniformMatrix4fv(g_modelLoc, 1, GL_FALSE, glm::value_ptr(itemModel));
                glUniform3f(g_colorLoc, 0.2f, 0.8f, 1.0f);
                drawCube();
            }
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
    glm::vec3 playerWorldPos(g_playerPosX, playerDrawY, g_playerPosZ);
    drawPacman(playerWorldPos);

    for (const Ghost& ghost : g_ghosts) {
        drawGhost(ghost);
    }
}

void display() {
    glm::vec3 lightPos = g_cameraPos;
    glUniform3fv(g_lightPosLoc, 1, glm::value_ptr(lightPos));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, g_windowWidth, g_windowHeight);

    glm::ivec2 gridPos = getGridCoord(g_playerPosX, g_playerPosZ);
    float tileY = 0.0f;
    if (gridPos.y >= 0 && gridPos.y < g_gridHeight && gridPos.x >= 0 && gridPos.x < g_gridWidth) {
        tileY = g_cubeCurrentHeight[gridPos.y][gridPos.x];
    }
    glm::vec3 playerWorldPos = glm::vec3(g_playerPosX, tileY, g_playerPosZ);

    float angleRad = glm::radians(g_playerAngleY);
    glm::vec3 forward(sin(angleRad), 0.0f, cos(angleRad));

    glm::vec3 camOffset = glm::vec3(0.0f, 8.0f, -6.0f);
    g_cameraPos = playerWorldPos + camOffset;
    g_cameraTarget = playerWorldPos + glm::vec3(0.0f, 1.0f, 0.0f);

    glm::mat4 view = glm::lookAt(g_cameraPos, g_cameraTarget, g_cameraUp);

    float aspect = (float)g_windowWidth / g_windowHeight;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // 메인 화면
    g_isMinimapView = false;
    drawGrid(view, projection, g_cameraPos);
    // --- Mini-map (top-right square) ---
    int padding = 10;
    int minimapSize = std::min(g_windowWidth, g_windowHeight) / 4; // 정사각형

    int x = g_windowWidth - minimapSize - padding;
    int y = g_windowHeight - minimapSize - padding;

    // 이 영역만 배경 색으로 칠하기
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, minimapSize, minimapSize);

    // 미니맵 배경 색 (조금 진한 회색 같은 느낌)
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 이제 이 영역에만 그리도록 뷰포트 설정
    glViewport(x, y, minimapSize, minimapSize);


    // 메인 화면은 그대로 두고, 미니맵 영역에서는 깊이만 초기화
    glClear(GL_DEPTH_BUFFER_BIT);

    // 미로 전체 범위 계산
    float totalGridWidth = (g_gridWidth - 1) * (CUBE_SIZE + GRID_SPACING);
    float totalGridHeight = (g_gridHeight - 1) * (CUBE_SIZE + GRID_SPACING);
    float halfW = totalGridWidth * 0.5f;
    float halfH = totalGridHeight * 0.5f;

    // 위에서 직각으로 내려다보는 카메라
    glm::mat4 minimapView = glm::lookAt(
        glm::vec3(0.0f, 30.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f) // 위쪽 기준
    );

    // 전체가 다 들어오도록 orthographic
    glm::mat4 minimapProj = glm::ortho(
        -halfW * 1.1f, halfW * 1.1f,
        -halfH * 1.1f, halfH * 1.1f,
        0.1f, 100.0f
    );

    // 미니맵 모드로 그리기
    g_isMinimapView = true;
    drawGrid(minimapView, minimapProj, glm::vec3(0.0f, 30.0f, 0.0f));
    g_isMinimapView = false;

    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);

    // ---- 2D Text Overlay ----
    float centerX = g_windowWidth * 0.5f;
    float centerY = g_windowHeight * 0.5f;

    switch (g_gameState) {
    case GameState::TITLE:
        renderText(centerX - 120.0f, centerY + 40.0f, "3D PAC-MAN (TEMP)");
        renderText(centerX - 150.0f, centerY - 10.0f, "PRESS ENTER OR SPACE TO START");
        renderText(centerX - 100.0f, centerY - 40.0f, "Q : QUIT");
        break;
    case GameState::PLAYING:
    {
        std::string hud = "SCORE: " + std::to_string(g_score) + "   LIVES: " + std::to_string(g_lives);
        renderText(20.0f, g_windowHeight - 30.0f, hud);

        if (g_ghostSlowActive) {
            std::string hud2 = "SLOW TIME: " + std::to_string((int)std::ceil(g_ghostSlowTimer));
            renderText(20.0f, g_windowHeight - 60.0f, hud2);
        }
    }
    break;
    case GameState::GAME_CLEAR:
        renderText(centerX - 80.0f, centerY + 10.0f, "STAGE CLEAR!");
        renderText(centerX - 180.0f, centerY - 30.0f, "R : RESTART   /   T : TITLE");
        break;
    case GameState::GAME_OVER:
        renderText(centerX - 80.0f, centerY + 10.0f, "GAME OVER");
        renderText(centerX - 180.0f, centerY - 30.0f, "R : RETRY     /   T : TITLE");
        break;
    }

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
    if (g_specialKeyStates[GLUT_KEY_LEFT] || g_keyStates['a'] || g_keyStates['A'])  moveVector += glm::vec3(1.0f, 0.0f, 0.0f);
    if (g_specialKeyStates[GLUT_KEY_RIGHT] || g_keyStates['d'] || g_keyStates['D']) moveVector -= glm::vec3(1.0f, 0.0f, 0.0f);

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

        float angleRad = std::atan2(moveVector.z, moveVector.x);
        g_playerAngleY = glm::degrees(angleRad) - 90.0f;

    }

    glm::ivec2 playerGrid = getGridCoord(g_playerPosX, g_playerPosZ);
    if (playerGrid.y >= 0 && playerGrid.y < g_gridHeight &&
        playerGrid.x >= 0 && playerGrid.x < g_gridWidth) {

        if (g_maze[playerGrid.y][playerGrid.x] == PATH && g_pellets[playerGrid.y][playerGrid.x]) {
            g_pellets[playerGrid.y][playerGrid.x] = false;

            g_remainingPellets--;
            g_score += 10;

            if (g_remainingPellets <= 0) {
                goToGameClear();
            }
        }

        if (g_maze[playerGrid.y][playerGrid.x] == PATH && g_slowItems[playerGrid.y][playerGrid.x]) {
            g_slowItems[playerGrid.y][playerGrid.x] = false;
            g_ghostSlowActive = true;
            g_ghostSlowTimer = GHOST_SLOW_DURATION;
            g_ghostSpeedScale = GHOST_SLOW_SCALE;
        }
    }
}

void updateGhosts(float deltaTime) {
    const float turnThreshold = 0.05f;

    auto isInside = [](int x, int z) {
        return x >= 0 && x < g_gridWidth && z >= 0 && z < g_gridHeight;
    };

    auto findNearestPath = [&](int gridX, int gridZ) {
        int maxRadius = std::max(g_gridWidth, g_gridHeight);
        for (int radius = 0; radius <= maxRadius; ++radius) {
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = gridX + dx;
                    int nz = gridZ + dz;
                    if (!isInside(nx, nz)) continue;
                    if (g_maze[nz][nx] == PATH) {
                        return glm::ivec2(nx, nz);
                    }
                }
            }
        }
        return glm::ivec2(gridX, gridZ);
    };

    for (Ghost& ghost : g_ghosts) {
        glm::ivec2 grid = getGridCoord(ghost.x, ghost.z);
        if (!isInside(grid.x, grid.y) || g_maze[grid.y][grid.x] == WALL) {
            glm::ivec2 nearest = findNearestPath(grid.x, grid.y);
            glm::vec3 nearestPos = getWorldPos(nearest.x, nearest.y);
            ghost.x = nearestPos.x;
            ghost.z = nearestPos.z;
            grid = nearest;
        }

        glm::vec3 cellCenter = getWorldPos(grid.x, grid.y);
        glm::vec2 ghostPos2D(ghost.x, ghost.z);
        glm::vec2 playerPos2D(g_playerPosX, g_playerPosZ);
        bool canTurn = glm::length(ghostPos2D - glm::vec2(cellCenter.x, cellCenter.z)) < turnThreshold;

        if (canTurn) {
            struct Candidate {
                int dx;
                int dz;
                bool isReverse;
                float distanceToPlayer;
            };

            std::vector<Candidate> candidates;
            const int dirX[4] = { 1, -1, 0, 0 };
            const int dirZ[4] = { 0, 0, 1, -1 };
            for (int i = 0; i < 4; ++i) {
                int nx = grid.x + dirX[i];
                int nz = grid.y + dirZ[i];
                if (!isInside(nx, nz) || g_maze[nz][nx] != PATH) continue;

                glm::vec3 nextCenter = getWorldPos(nx, nz);
                float distanceToPlayer = glm::length(glm::vec2(nextCenter.x, nextCenter.z) - playerPos2D);
                bool isReverse = (dirX[i] == -ghost.dirX && dirZ[i] == -ghost.dirZ);

                candidates.push_back({ dirX[i], dirZ[i], isReverse, distanceToPlayer });
            }

            if (!candidates.empty()) {
                std::vector<Candidate> bestCandidates;

                auto evaluateCandidates = [&](bool allowReverse) {
                    float localBest = std::numeric_limits<float>::max();
                    std::vector<Candidate> localCandidates;
                    for (const Candidate& c : candidates) {
                        if (!allowReverse && c.isReverse) continue;
                        if (c.distanceToPlayer < localBest - 1e-4f) {
                            localBest = c.distanceToPlayer;
                            localCandidates.clear();
                            localCandidates.push_back(c);
                        }
                        else if (std::abs(c.distanceToPlayer - localBest) < 1e-4f) {
                            localCandidates.push_back(c);
                        }
                    }
                    return std::make_pair(localBest, localCandidates);
                };

                auto nonReverseResult = evaluateCandidates(false);
                if (!nonReverseResult.second.empty()) {
                    bestCandidates = nonReverseResult.second;
                }
                else {
                    auto anyResult = evaluateCandidates(true);
                    bestCandidates = anyResult.second;
                }

                if (!bestCandidates.empty()) {
                    std::uniform_int_distribution<size_t> dist(0, bestCandidates.size() - 1);
                    const Candidate& chosen = bestCandidates[dist(g_randomEngine)];
                    ghost.dirX = chosen.dx;
                    ghost.dirZ = chosen.dz;
                }
            }
        }

        float moveSpeed = (ghost.speed > 0.0f ? ghost.speed : GHOST_MOVE_SPEED) * g_ghostSpeedScale;
        ghost.x += ghost.dirX * moveSpeed * deltaTime;
        ghost.z += ghost.dirZ * moveSpeed * deltaTime;

        if (ghost.dirX != 0 || ghost.dirZ != 0) {
            float angleRad = std::atan2(static_cast<float>(ghost.dirX), static_cast<float>(ghost.dirZ));
            ghost.angleY = glm::degrees(angleRad);
        }

        float dx = ghost.x - g_playerPosX;
        float dz = ghost.z - g_playerPosZ;
        float dist2 = dx * dx + dz * dz;
        const float collisionDistance = 0.4f;

        if (dist2 < collisionDistance * collisionDistance) {
            g_lives--;
            if (g_lives <= 0) {
                goToGameOver();
            }
            else {
                reset();
                g_gameState = GameState::PLAYING;
            }
            return;
        }
    }
}

void update(int value) {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - g_lastTime) / 1000.0f;
    if (deltaTime < 0.001f) deltaTime = 0.001f;
    g_lastTime = currentTime;

    auto animatePacmanMouth = [&](float dt) {
        g_pacmanMouthAngle += g_pacmanMouthDir * PACMAN_MOUTH_SPEED * dt;

        if (g_pacmanMouthAngle > PACMAN_MOUTH_MAX) {
            g_pacmanMouthAngle = PACMAN_MOUTH_MAX;
            g_pacmanMouthDir = -1.0f;
        }
        else if (g_pacmanMouthAngle < 0.0f) {
            g_pacmanMouthAngle = 0.0f;
            g_pacmanMouthDir = 1.0f;
        }
    };

    if (g_ghostSlowActive) {
        g_ghostSlowTimer -= deltaTime;
        if (g_ghostSlowTimer <= 0.0f) {
            g_ghostSlowActive = false;
            g_ghostSlowTimer = 0.0f;
            g_ghostSpeedScale = 1.0f;
        }
    }

    if (g_gameState == GameState::PLAYING) {
        handlePlayerInput(deltaTime);
        updateGhosts(deltaTime);
    }

    // 팩맨 입 애니메이션: 게임 진행 여부와 관계없이 계속 반복
    animatePacmanMouth(deltaTime);

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void keyboard(unsigned char key, int x, int y) {
    g_keyStates[key] = true;

    // 공통 종료 키
    if (key == 'q' || key == 'Q') {
        glutLeaveMainLoop();
        return;
    }

    switch (g_gameState) {
    case GameState::TITLE:
        if (key == 13 || key == ' ') {
            startNewGame();
        }
        break;

    case GameState::PLAYING:
        if (key == 'k' || key == 'K') {
            goToGameOver();
        }
        else if (key == 'v' || key == 'V') {
            goToGameClear();
        }
        else {
            switch (key) {
            case 27:
                glutLeaveMainLoop();
                break;
            case 'c': case 'C':
                reset();
                break;
            }
        }
        break;

    case GameState::GAME_OVER:
        if (key == 'r' || key == 'R') {
            startNewGame();
        }
        else if (key == 't' || key == 'T') {
            goToTitle();
        }
        break;

    case GameState::GAME_CLEAR:
        if (key == 'n' || key == 'N') {
            if (g_currentStage < MAX_STAGE) {
                g_currentStage++;
                reset();
                g_gameState = GameState::PLAYING;
            }
        }
        else if (key == 'r' || key == 'R') {
            reset();
            g_gameState = GameState::PLAYING;
        }
        else if (key == 't' || key == 'T') {
            goToTitle();
        }
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
    glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
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