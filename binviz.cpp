#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <math.h>

#define PI 3.14159265358979323846264f
#define WINDOW_TITLE "binviz"
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

struct Point {
  float x, y, z;
  int count;  // count will be used to color each point

  Point(float X, float Y, float Z) {
    x = X;
    y = Y;
    z = Z;
  }
};

std::vector<std::unique_ptr<Point> > points;

int winWidth, winHeight;
double lastMouseX, lastMouseY;
double zoomLevel = 1;
double zoomTarget = 1;
float rotX, rotY;
float targetRotX, targetRotY;
bool mouseDown = false;

static void errorCallback(int error, const char* desc) {
  std::cerr << desc << std::endl;
}

static void keyCallback(GLFWwindow* wind, int key, int sc, int action,
                        int mods) {
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_ESCAPE) {
      glfwSetWindowShouldClose(wind, GL_TRUE);
    }
  }
}

static void cursorCallback(GLFWwindow* wind, double x, double y) {
  if (mouseDown) {
    // Rotate points when
    targetRotX -= (x - lastMouseX) / winWidth * 100;
    targetRotY -= (y - lastMouseY) / winHeight * 100;
  }
  lastMouseX = x;
  lastMouseY = y;
}

static void mouseButtonCallback(GLFWwindow* wind, int button, int action,
                                int mod) {
  mouseDown = (action == GLFW_PRESS);
}

static void scrollCallback(GLFWwindow* wind, double xOffset, double yOffset) {
  // shift speeds up zoom
  if (glfwGetKey(wind, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    zoomTarget += yOffset;
  } else {
    zoomTarget += (yOffset / 25.f);
  }

  if (zoomTarget < 0.1) {  // don't let zoom level get too low
    zoomTarget = 0.1;
  }
}

int loadFile(const char* filename) {
  points.clear();
  std::ifstream is(filename, std::ifstream::binary);

  if (is) {
    is.seekg(0, is.end);
    int length = is.tellg();
    is.seekg(0, is.beg);

    char* buffer = new char[length];
    std::cout << "Reading " << length << " bytes" << std::endl;
    is.read(buffer, length);
    is.close();

    int* sortBuffer =
        new int[length / 3];  // convert each 3 byte pair to an int to sort them
    for (int i = 0; i + 2 < length; i += 3) {
      // pack them into an int
      int x = buffer[i + 2];
      x = (x << 8) + buffer[i + 1];
      x = (x << 8) + buffer[i];
      sortBuffer[i / 3] = x;
    }

    length /= 3;
    // sorting will ensure that duplicate points are next to each other
    std::sort(sortBuffer, sortBuffer + length);

    int duplicate = 0;
    for (int i = 0; i < length; i++) {
      int packed = sortBuffer[i];
      if (i > 0) {
        if (packed == sortBuffer[i - 1]) {
          duplicate++;
          points.back()->count++;
          continue;
        }
      }

      // unpack bytes and normalize
      float x = ((packed >> 16) & 0xFF) / 255.f;
      float y = ((packed >> 8) & 0xFF) / 255.f;
      float z = (packed & 0xFF) / 255.f;

      // use x and y as rotation values
      // and z as the radius
      float xRot = x * (2 * PI);
      float yRot = y * (2 * PI);
      float radius = z;

      // convert the spherical coordinates into the cartesian plane
      // http://en.wikipedia.org/wiki/Spherical_coordinate_system
      x = radius * sin(xRot) * cos(yRot);
      y = radius * sin(xRot) * sin(yRot);
      z = radius * cos(xRot);

      points.emplace_back(new Point(x, y, z));
    }
    delete sortBuffer;
    delete buffer;
    return length;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " [file]" << std::endl;
    exit(EXIT_FAILURE);
  }

  int length = loadFile(argv[1]);
  std::cout << points.size() << " unique points" << std::endl;

  if (length == 0) {
    std::cout << "Unable to open file or file is empty." << std::endl;
    exit(EXIT_FAILURE);
  }

  GLFWwindow* window;
  glfwSetErrorCallback(errorCallback);

  if (!glfwInit()) {
    std::cerr << "GLFW failed to initialize, exiting..." << std::endl;
    exit(EXIT_FAILURE);
  }

  window =
      glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);

  if (!window) {
    std::cerr << "Failed to create window, exiting..." << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);

  // Set all callbacks
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, cursorCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetScrollCallback(window, scrollCallback);

  // makes points circles instead of squares
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_BLEND);

  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {  // render loop
    double deltaTime = glfwGetTime() - lastTime;
    lastTime = glfwGetTime();

    // smooth transition to target zoom level and rotation
    zoomLevel += (zoomTarget - zoomLevel) * deltaTime;
    rotX += (targetRotX - rotX) * deltaTime;
    rotY += (targetRotY - rotY) * deltaTime;

    glfwGetFramebufferSize(window, &winWidth, &winHeight);
    float ratio = winWidth / (float)winHeight;

    // I'm not good at OpenGL
    glViewport(0, 0, winWidth, winHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ratio, ratio, -1.f, 1.f, 5.f, -5.f);

    glMatrixMode(GL_MODELVIEW);
    glRotatef(rotY, 1.f, 0.f, 0.f);
    glRotatef(rotX, 0.f, 1.f, 0.f);
    glRotatef(-45, 1.f, 0.f, 1.f);

    // Scales all points by zoomLevel (better way to do this?)
    glScalef(zoomLevel, zoomLevel, zoomLevel);

    glPointSize(1.0f);  // all points are 1 pixel
    glBegin(GL_POINTS);
    for (int i = 0; i < points.size(); i++) {
      Point p = *points[i];
      // color roughly on number of duplicate points
      float count = p.count / 10.f;
      // also color based on distance from the beginning of the file
      float color = i / (float)points.size();
      // this is weird, better way of coloring?
      glColor3f(color, 1.0f - count, 1.0f - (count * color));
      glVertex3f(p.x, p.y, p.z);
    }
    glEnd();

    glFlush();
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  exit(EXIT_SUCCESS);
}
