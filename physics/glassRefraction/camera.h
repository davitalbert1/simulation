#pragma once

// Camera orbital usada para inspecionar a bancada optica em tres dimensoes.
// A posicao do olho e derivada de yaw/pitch/distancia ao redor de um alvo (target).
struct Camera {
    float yaw = 42.0f;
    float pitch = 26.0f;
    float distance = 8.2f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
};

void ApplyCamera(const Camera& cam);