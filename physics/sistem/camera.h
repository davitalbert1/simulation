#pragma once

struct Camera {
    float distance = 10.0f;
    float angleX = 0.0f;
    float angleY = 0.0f;
};

void ApplyCamera(const Camera& cam);