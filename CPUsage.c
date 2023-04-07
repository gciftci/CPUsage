#include <stdio.h>
#include <windows.h>
#include <pdh.h>
#include <GL/glut.h>
#include <GL/freeglut.h>

#define MAX_HISTORY 500 // Maximum number of data points to store in the history buffer
#define UPDATE_INTERVAL 1 // Update interval in milliseconds
#define MAX_CORES 32 // Maximum number of CPU cores to show
#define WINDOW_SIZE (MAX_HISTORY / 5)  // Define the window size for the moving average

// Declare global variables
PDH_HQUERY cpuQuery;
PDH_HCOUNTER cpuCores[MAX_CORES];
int CORE_GRAPH_COLS = 6;
int CORE_GRAPH_ROWS = 2;
double cpuHistory[MAX_CORES][MAX_HISTORY] = {{0}};
int cpuHistoryIndex = 0;
int numCores = 0;

// movingAverage for smoothing out data points (CPU-Loads) over a period of time (WINDOW_SIZE)
double movingAverage(int core, double newValue) {
    static double window[MAX_CORES][WINDOW_SIZE] = {{0}};
    static int windowIndex[MAX_CORES] = {0};
    static int windowSize[MAX_CORES] = {0};

    window[core][windowIndex[core]] = newValue;
    windowIndex[core] = (windowIndex[core] + 1) % WINDOW_SIZE;

    if (windowSize[core] < WINDOW_SIZE) {
        windowSize[core]++;
    }

    double sum = 0;
    for (int i = 0; i < windowSize[core]; i++) {
        sum += window[core][i];
    }

    return sum / windowSize[core];
}

// Get the number of Cores
void initCores() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    numCores = sysInfo.dwNumberOfProcessors;
    CORE_GRAPH_COLS = (numCores / 2);
    CORE_GRAPH_ROWS = 2;
    if (numCores > MAX_CORES) {
        numCores = MAX_CORES;
    }
}

// Update the CPU-Usage-Data (cpuHistory[CORES][MAX_HISTORY])
void updateCpuUsage(int value)
{
    int frameCount = 0;
    int currentTime = 0;
    int previousTime = 0;
    cpuHistoryIndex = (cpuHistoryIndex + 1) % MAX_HISTORY;
    for (int i = 0; i < MAX_CORES; i++) {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuCores[i], PDH_FMT_DOUBLE, NULL, &counterVal);
        //printf("Core #%d: Raw Value: %.2f%%\n", i, counterVal.doubleValue); // Print raw value for debugging
        cpuHistory[i][cpuHistoryIndex] = movingAverage(i, counterVal.doubleValue);
        //printf("Core #%d: EMA Value: %.2f%%\n", i, cpuHistory[i][cpuHistoryIndex]); // Print EMA value for debugging
    }
    // Update the frame count and time
    frameCount++;
    currentTime = glutGet(GLUT_ELAPSED_TIME);

    // Calculate the FPS and print it to the console every second
    if (currentTime - previousTime >= 1000) {
        float fps = frameCount / ((currentTime - previousTime) / 1000.0f);
        printf("FPS: %.2f\n", fps);
        frameCount = 0;
        previousTime = currentTime;
    }
    // Redraw the window
    glutPostRedisplay();

    // Register the update function to be called again after the specified interval
    glutTimerFunc(UPDATE_INTERVAL, updateCpuUsage, 0);
}

void drawAxes(void) {
    glColor4f(0.5, 0.5, 0.5, 1.0);
    glBegin(GL_LINES);
    glVertex2f(0.0, 0.0);
    glVertex2f(1.0, 0.0);
    glVertex2f(0.0, 0.0);
    glVertex2f(0.0, 1.0);
    glEnd();
}

void drawTotalCpuUsageHistory() {
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_LINE_STRIP);
    for (int j = 0; j < MAX_HISTORY; j++) {
        double totalCpuUsage = 0;
        for (int i = 0; i < numCores; i++) {
            totalCpuUsage += cpuHistory[i][(cpuHistoryIndex + j) % MAX_HISTORY];
        }
        totalCpuUsage /= numCores;
        glVertex2f((float)j / MAX_HISTORY, totalCpuUsage / 100.0);
    }
    glEnd();
}

void drawCpuUsageHistory(int core) {
    glColor3f(0.0, 1.0, 0.0);
    glBegin(GL_LINE_STRIP);
    for (int j = 0; j < MAX_HISTORY; j++) {
        glVertex2f((float)j / MAX_HISTORY, cpuHistory[core][(cpuHistoryIndex + j) % MAX_HISTORY] / 100.0);
    }
    glEnd();
}

void drawCoreLabel(int core) {
    glColor3f(1.0, 1.0, 1.0);
    glRasterPos2f(0.01, 0.9);
    char label[32];
    sprintf(label, "CPU-%d", core);
    const char *c = label;
    while (*c) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c++);
    }
}

// Add padding parameters to the drawGrid function
void drawGrid(float paddingX, float paddingY) {
    glColor4f(0.2, 0.2, 0.2, 0.9);
    glBegin(GL_LINES);

    // Draw vertical lines with padding
    for (float x = paddingX; x <= 1.0f - paddingX; x += (1.0f - 2 * paddingX) / 6) {
        glVertex2f(x, paddingY);
        glVertex2f(x, 1.0f - paddingY);
    }

    // Draw horizontal lines with padding
    for (float y = paddingY; y <= 1.0f - paddingY; y += (1.0f - 2 * paddingY) / 10.0f) {
        glVertex2f(paddingX, y);
        glVertex2f(1.0f - paddingX, y);
    }

    glEnd();
}

void drawTotalCpuLabel() {
    glColor3f(1.0, 1.0, 1.0);
    glRasterPos2f(0.01, 0.9);
    const char *label = "CPU-Total";
    while (*label) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *label++);
    }
}

void display(void) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);

    // Calculate the dimensions of each core graph
    float coreWidth = 1.0f / CORE_GRAPH_COLS;
    float coreHeight = 1.0f / (CORE_GRAPH_ROWS + 1);
    float corePaddingX = coreWidth * 0.05f;
    float corePaddingY = coreHeight * 0.1f;

    // Draw individual core graphs
    for (int i = 0; i < numCores; i++) {
        int row = i / CORE_GRAPH_COLS;
        int col = i % CORE_GRAPH_COLS;
        float x = col * coreWidth;
        float y = row * coreHeight;

        glViewport(x * glutGet(GLUT_WINDOW_WIDTH) + corePaddingX * glutGet(GLUT_WINDOW_WIDTH),
                   y * glutGet(GLUT_WINDOW_HEIGHT) + corePaddingY * glutGet(GLUT_WINDOW_HEIGHT),
                   coreWidth * glutGet(GLUT_WINDOW_WIDTH) - 2 * corePaddingX * glutGet(GLUT_WINDOW_WIDTH),
                   coreHeight * glutGet(GLUT_WINDOW_HEIGHT) - 2 * corePaddingY * glutGet(GLUT_WINDOW_HEIGHT));

        // Draw axes, grid, and CPU usage history
        drawAxes();
        drawGrid(corePaddingX, corePaddingY);
        drawCpuUsageHistory(i);
        drawCoreLabel(i);
    }

    // Calculate the dimensions of the total CPU usage graph
    float totalWidth = 1.0f;
    float totalHeight = 1.0f / (CORE_GRAPH_ROWS + 1);
    float totalPaddingX = totalWidth * 0.1f;
    float totalPaddingY = totalHeight * 0.1f;

    // Draw total CPU usage graph
    glViewport(0,
               (CORE_GRAPH_ROWS * coreHeight) * glutGet(GLUT_WINDOW_HEIGHT) + totalPaddingY * glutGet(GLUT_WINDOW_HEIGHT),
               totalWidth * glutGet(GLUT_WINDOW_WIDTH),
               totalHeight * glutGet(GLUT_WINDOW_HEIGHT) - 2 * totalPaddingY * glutGet(GLUT_WINDOW_HEIGHT));

    // Draw axes, grid, and CPU usage history
    drawAxes();
    //drawGrid(totalPaddingX, totalPaddingY);
    drawTotalCpuUsageHistory();
    drawTotalCpuLabel();

    glutSwapBuffers();
}

int main(int argc, char** argv)
{
    // Initialize the number of cores
    initCores();

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1200, 250);
    glutCreateWindow("CPU Usage Graph");

    // Initialize OpenGL
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);

    // Add the total CPU load counter
    PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuCores[numCores]);

    // Initialize the performance counter
    PdhOpenQuery(NULL, 0, &cpuQuery);

    for (int i = 0; i < MAX_CORES; i++) {
        char counterPath[256];
        sprintf(counterPath, "\\Processor(%d)\\%% Processor Time", i);
        PdhAddCounter(cpuQuery, counterPath, 0, &cpuCores[i]);
    }

    PdhCollectQueryData(cpuQuery);

    // Register the update function to be called after the specified interval
    glutTimerFunc(UPDATE_INTERVAL, updateCpuUsage, 0);

    // Start the main loop
    glutDisplayFunc(display);
    glutMainLoop();

    // Cleanup
    for (int i = 0; i < MAX_CORES; i++) {
        PdhRemoveCounter(cpuCores[i]);
    }
    PdhCloseQuery(cpuQuery);

    return 0;
}