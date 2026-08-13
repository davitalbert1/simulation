#include "render.h"
#include "window.h"
#include "simulation.h"
#include "camera.h"
#include <GL/gl.h>
#include <GL/glu.h>

extern Camera camera;

namespace {
void DrawGroundPlane() {
    glColor3f(0.15f, 0.20f, 0.25f);
    glBegin(GL_LINES);
    for (float i = -50.0f; i <= 50.0f; i += 5.0f) {
        glVertex3f(i, 0.0f, -50.0f);
        glVertex3f(i, 0.0f, 50.0f);
        glVertex3f(-50.0f, 0.0f, i);
        glVertex3f(50.0f, 0.0f, i);
    }
    glEnd();

    glColor3f(0.25f, 0.30f, 0.35f);
    glBegin(GL_QUADS);
    glVertex3f(-50.0f, -0.05f, -50.0f);
    glVertex3f(50.0f, -0.05f, -50.0f);
    glVertex3f(50.0f, -0.05f, 50.0f);
    glVertex3f(-50.0f, -0.05f, 50.0f);
    glEnd();
}

void DrawFood(const Food& food) {
    glPushMatrix();
    glTranslatef(food.x, 0.9f, food.z);
    glColor3f(0.2f, 0.85f, 0.2f);

    glBegin(GL_QUADS);
    glVertex3f(0.0f, 1.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, -1.0f);
    glEnd();

    glPopMatrix();
}

void DrawCritter(const Critter& critter) {
    glPushMatrix();
    glTranslatef(critter.x, 0.8f, critter.z);
    glColor3f(critter.genes.r, critter.genes.g, critter.genes.b);

    glBegin(GL_QUADS);
    glVertex3f(-0.8f, -0.8f, 0.8f);
    glVertex3f(0.8f, -0.8f, 0.8f);
    glVertex3f(0.8f, 0.8f, 0.8f);
    glVertex3f(-0.8f, 0.8f, 0.8f);

    glVertex3f(-0.8f, -0.8f, -0.8f);
    glVertex3f(-0.8f, 0.8f, -0.8f);
    glVertex3f(0.8f, 0.8f, -0.8f);
    glVertex3f(0.8f, -0.8f, -0.8f);

    glVertex3f(-0.8f, 0.8f, -0.8f);
    glVertex3f(-0.8f, 0.8f, 0.8f);
    glVertex3f(0.8f, 0.8f, 0.8f);
    glVertex3f(0.8f, 0.8f, -0.8f);

    glVertex3f(-0.8f, -0.8f, -0.8f);
    glVertex3f(0.8f, -0.8f, -0.8f);
    glVertex3f(0.8f, -0.8f, 0.8f);
    glVertex3f(-0.8f, -0.8f, 0.8f);

    glVertex3f(0.8f, -0.8f, -0.8f);
    glVertex3f(0.8f, 0.8f, -0.8f);
    glVertex3f(0.8f, 0.8f, 0.8f);
    glVertex3f(0.8f, -0.8f, 0.8f);

    glVertex3f(-0.8f, -0.8f, -0.8f);
    glVertex3f(-0.8f, -0.8f, 0.8f);
    glVertex3f(-0.8f, 0.8f, 0.8f);
    glVertex3f(-0.8f, 0.8f, -0.8f);
    glEnd();

    glPopMatrix();
}
}

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
    ApplyCamera(camera);

    DrawGroundPlane();

    for (const auto& food : foods) {
        DrawFood(food);
    }

    for (const auto& critter : critters) {
        DrawCritter(critter);
    }
}