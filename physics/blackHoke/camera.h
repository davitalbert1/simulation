#pragma once

struct Camera {
    float yaw = 35.0f;
    float pitch = 18.0f;
    float distance = 9.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
};

void ApplyCamera(const Camera& cam);
