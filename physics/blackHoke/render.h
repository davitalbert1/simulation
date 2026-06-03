#pragma once

struct Vec3 {
    float x, y, z;
};

void InitScene();
void DrawBlackHoleScene(float time, const Vec3& lightPos);
