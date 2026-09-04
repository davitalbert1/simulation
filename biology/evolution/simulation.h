#pragma once
#include <vector>

struct Genes {
    float speed = 5.0f;
    float sightRadius = 20.0f;
    float r = 0.45f;
    float g = 0.72f;
    float b = 0.45f;
};

struct Critter {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float energy = 100.0f;
    float directionX = 0.0f;
    float directionZ = 0.0f;
    float wanderTimer = 0.0f;
    Genes genes;
};

struct Food {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

void ResetSimulation();
void UpdateSimulation(float deltaTime);

extern std::vector<Critter> critters;
extern std::vector<Food> foods;