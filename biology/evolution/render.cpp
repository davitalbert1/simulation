#include "render.h"
#include "window.h"
#include "simulation.h"
#include <GL/gl.h>
#include <GL/glu.h>

void InitRender() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void SetupProjection(int width, int height) {
    if (height == 0) height = 1;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 1000.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void DrawGLScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // TODO: Apply camera transformations here

    // Draw food (as green points)
    glPointSize(5.0f);
    glColor3f(0.2f, 0.8f, 0.2f);
    glBegin(GL_POINTS);
    for (const auto& food : foods) {
        glVertex3f(food.x, food.y, food.z);
    }
    glEnd();

    // Draw critters (as blue points)
    glPointSize(8.0f);
    glColor3f(0.3f, 0.6f, 1.0f);
    glBegin(GL_POINTS);
    for (const auto& critter : critters) {
        glVertex3f(critter.x, critter.y, critter.z);
    }
    glEnd();
}