#include "render.h"
#include <GL/gl.h>
#include <GL/glu.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <vector>

struct Star {
    float x, y, z;
    float brightness;
};

static std::vector<Star> stars;

static float Clamp(float v, float a, float b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

static void SetMaterial(float r, float g, float b, float emission = 0.0f) {
    GLfloat diffuse[] = {r, g, b, 1.0f};
    GLfloat ambient[] = {r * 0.15f, g * 0.15f, b * 0.15f, 1.0f};
    GLfloat specular[] = {0.15f, 0.15f, 0.15f, 1.0f};
    GLfloat emit[] = {r * emission, g * emission, b * emission, 1.0f};

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emit);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 12.0f);
}

static void DrawSphere(float radius, int stacks = 32, int slices = 32) {
    for (int i = 0; i < stacks; i++) {
        float lat0 = (float)M_PI * (-0.5f + (float)i / stacks);
        float lat1 = (float)M_PI * (-0.5f + (float)(i + 1) / stacks);

        float z0 = sinf(lat0);
        float zr0 = cosf(lat0);
        float z1 = sinf(lat1);
        float zr1 = cosf(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * (float)M_PI * (float)j / slices;
            float x = cosf(lng);
            float y = sinf(lng);

            glNormal3f(x * zr0, y * zr0, z0);
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            glNormal3f(x * zr1, y * zr1, z1);
            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }
}

static void DrawStars() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glPointSize(2.0f);

    glBegin(GL_POINTS);
    for (const Star& s : stars) {
        float c = s.brightness;
        glColor3f(c, c, c * 1.12f);
        glVertex3f(s.x, s.y, s.z);
    }
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glPopAttrib();
}

static void DrawAccretionDisk(float time) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    const int rings = 18;
    const int segments = 160;
    const float inner = 1.35f;
    const float outer = 4.10f;

    for (int r = 0; r < rings; r++) {
        float t0 = (float)r / rings;
        float t1 = (float)(r + 1) / rings;
        float radius0 = inner + (outer - inner) * t0;
        float radius1 = inner + (outer - inner) * t1;

        float heat0 = 1.0f - t0;
        float heat1 = 1.0f - t1;

        glBegin(GL_QUAD_STRIP);

        for (int i = 0; i <= segments; i++) {
            float a = 2.0f * (float)M_PI * i / segments;
            float swirl = time * (1.8f + heat0 * 2.2f);
            float wave0 = sinf(a * 5.0f - swirl) * 0.035f * radius0;
            float wave1 = sinf(a * 5.0f - swirl) * 0.035f * radius1;

            float x0 = cosf(a) * radius0;
            float z0 = sinf(a) * radius0;
            float x1 = cosf(a) * radius1;
            float z1 = sinf(a) * radius1;

            float alpha0 = 0.16f + heat0 * 0.45f;
            float alpha1 = 0.16f + heat1 * 0.45f;

            glColor4f(1.0f, 0.28f + heat0 * 0.45f, 0.04f, alpha0);
            glVertex3f(x0, wave0, z0);

            glColor4f(0.35f + heat1 * 0.65f, 0.55f + heat1 * 0.20f, 1.0f - heat1 * 0.55f, alpha1);
            glVertex3f(x1, wave1, z1);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
    glPopAttrib();
}

static void DrawPhotonSphere() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(2.0f);

    glColor4f(0.35f, 0.75f, 1.0f, 0.35f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 180; i++) {
        float a = 2.0f * (float)M_PI * i / 180.0f;
        glVertex3f(cosf(a) * 1.52f, 0.0f, sinf(a) * 1.52f);
    }
    glEnd();

    glPopAttrib();
}

static void DrawLightRays(const Vec3& lightPos, float time) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.4f);

    // quantidade de raios de luz (multiplicado por 2)
    for (int ray = -3; ray <= 50; ray++) {
        if (ray == 0) continue;

        float offset = ray * 0.34f;
        glColor4f(1.0f, 0.82f, 0.38f, 0.22f);
        glBegin(GL_LINE_STRIP);

        for (int i = 0; i <= 80; i++) {
            float u = (float)i / 80.0f;
            float angle = offset * (1.0f - u) + u * (2.0f + 0.18f * sinf(time + ray));
            float radius = 0.85f + u * 4.8f;
            float bend = 1.0f / (radius + 0.2f);
            float x = lightPos.x * (1.0f - u) + cosf(angle + bend * ray) * radius * u;
            float y = lightPos.y * (1.0f - u) + sinf(u * (float)M_PI) * offset * 0.45f;
            float z = lightPos.z * (1.0f - u) + sinf(angle + bend * ray) * radius * u;

            glVertex3f(x, y, z);
        }

        glEnd();
    }

    glPopAttrib();
}

static void DrawLightSource(const Vec3& lightPos) {
    glPushMatrix();
    glTranslatef(lightPos.x, lightPos.y, lightPos.z);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glColor4f(1.0f, 0.78f, 0.25f, 0.20f);
    DrawSphere(0.55f, 18, 18);

    glColor4f(1.0f, 0.92f, 0.55f, 1.0f);
    DrawSphere(0.16f, 18, 18);

    glPopAttrib();
    glPopMatrix();
}

void InitScene() {
    stars.clear();
    srand(42);

    for (int i = 0; i < 1300; i++) {
        float theta = ((rand() % 10000) / 10000.0f) * 2.0f * (float)M_PI;
        float phi = ((rand() % 10000) / 10000.0f) * (float)M_PI;
        float r = 36.0f;

        Star s;
        s.x = sinf(phi) * cosf(theta) * r;
        s.y = cosf(phi) * r;
        s.z = sinf(phi) * sinf(theta) * r;
        s.brightness = 0.25f + (rand() % 100) / 140.0f;
        stars.push_back(s);
    }
}

void DrawBlackHoleScene(float time, const Vec3& lightPos) {
    DrawStars();

    GLfloat globalAmbient[] = {0.025f, 0.025f, 0.035f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    GLfloat light[] = {lightPos.x, lightPos.y, lightPos.z, 1.0f};
    GLfloat diffuse[] = {1.0f, 0.78f, 0.35f, 1.0f};
    GLfloat specular[] = {1.0f, 0.88f, 0.55f, 1.0f};

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.7f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.03f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.005f);

    DrawAccretionDisk(time);
    DrawPhotonSphere();
    DrawLightRays(lightPos, time);
    DrawLightSource(lightPos);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    SetMaterial(0.0f, 0.0f, 0.0f, 0.0f);
    glColor3f(0.0f, 0.0f, 0.0f);
    DrawSphere(0.92f, 48, 48);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(0.12f, 0.38f, 0.62f, 0.18f);
    DrawSphere(1.05f, 40, 40);
    glPopAttrib();
}
