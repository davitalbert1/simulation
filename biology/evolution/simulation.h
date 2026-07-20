#pragma once

#include <vector>

struct Critter {
    float x, y, z;
    // Add other properties like energy, genes, etc.
};

struct Food {
    float x, y, z;
};

void ResetSimulation();
void UpdateSimulation(float deltaTime);

extern std::vector<Critter> critters;
extern std::vector<Food> foods;