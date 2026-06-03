#include "camera.h"
#include <gl/gl.h>
#include <cmath>

void ApplyCamera(const Camera& cam) {
    glTranslatef(0, 0, -cam.distance);
    glRotatef(cam.angleX, 1, 0, 0);
    glRotatef(cam.angleY, 0, 1, 0);
}