#pragma once

struct Camera {
    float yaw = -90.0f;
    float pitch = -15.0f;
    float distance = 50.0f;
    float targetX = 0.0f, targetY = 0.0f, targetZ = 0.0f;
};

void ApplyCamera(const Camera& cam);