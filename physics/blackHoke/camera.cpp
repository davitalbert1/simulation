#include "camera.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <GL/glu.h>

static float DegToRad(float deg) {
    return deg * (float)M_PI / 180.0f;
}

void ApplyCamera(const Camera& cam) {
    float yaw = DegToRad(cam.yaw);
    float pitch = DegToRad(cam.pitch);

    float cp = cosf(pitch);
    float eyeX = cam.targetX + cam.distance * cp * sinf(yaw);
    float eyeY = cam.targetY + cam.distance * sinf(pitch);
    float eyeZ = cam.targetZ + cam.distance * cp * cosf(yaw);

    gluLookAt(
        eyeX, eyeY, eyeZ,
        cam.targetX, cam.targetY, cam.targetZ,
        0.0f, 1.0f, 0.0f
    );
}
