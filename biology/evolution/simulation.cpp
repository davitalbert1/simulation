#include "simulation.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
constexpr float WORLD_HALF_SIZE = 50.0f;
constexpr float MAX_CRITTERS = 200.0f;
constexpr float MAX_FOOD = 80.0f;
constexpr float FOOD_REGEN_INTERVAL = 1.5f;

float RandomRange(float minValue, float maxValue) {
    return minValue + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (maxValue - minValue);
}

float Clamp(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

void NormalizeDirection(float& x, float& z) {
    float length = std::sqrt(x * x + z * z);
    if (length > 0.0001f) {
        x /= length;
        z /= length;
    } else {
        x = 1.0f;
        z = 0.0f;
    }
}
}

std::vector<Critter> critters;
std::vector<Food> foods;

void ResetSimulation() {
    critters.clear();
    foods.clear();

    const int initialCritters = 18;
    const int initialFood = 40;

    for (int i = 0; i < initialCritters; ++i) {
        Critter critter;
        critter.x = RandomRange(-WORLD_HALF_SIZE, WORLD_HALF_SIZE);
        critter.z = RandomRange(-WORLD_HALF_SIZE, WORLD_HALF_SIZE);
        critter.y = 0.0f;
        critter.energy = 100.0f;
        critter.genes.speed = RandomRange(3.0f, 8.0f);
        critter.genes.sightRadius = RandomRange(12.0f, 26.0f);
        critter.genes.r = RandomRange(0.2f, 1.0f);
        critter.genes.g = RandomRange(0.1f, 1.0f);
        critter.genes.b = RandomRange(0.2f, 1.0f);
        critter.directionX = RandomRange(-1.0f, 1.0f);
        critter.directionZ = RandomRange(-1.0f, 1.0f);
        NormalizeDirection(critter.directionX, critter.directionZ);
        critter.wanderTimer = RandomRange(0.5f, 2.0f);
        critters.push_back(critter);
    }

    for (int i = 0; i < initialFood; ++i) {
        Food food;
        food.x = RandomRange(-WORLD_HALF_SIZE, WORLD_HALF_SIZE);
        food.z = RandomRange(-WORLD_HALF_SIZE, WORLD_HALF_SIZE);
        food.y = 0.0f;
        foods.push_back(food);
    }
}

void UpdateSimulation(float deltaTime) {
    if (deltaTime <= 0.0f) {
        return;
    }

    static float foodTimer = FOOD_REGEN_INTERVAL;
    foodTimer -= deltaTime;

    auto removeDeadCritters = std::remove_if(critters.begin(), critters.end(), [](const Critter& critter) {
        return critter.energy <= 0.0f;
    });
    critters.erase(removeDeadCritters, critters.end());

    for (auto& critter : critters) {
        int nearestFoodIndex = -1;
        float nearestDistance = std::numeric_limits<float>::max();

        for (std::size_t i = 0; i < foods.size(); ++i) {
            const Food& food = foods[i];
            float dx = food.x - critter.x;
            float dz = food.z - critter.z;
            float distance = std::sqrt(dx * dx + dz * dz);

            if (distance <= critter.genes.sightRadius && distance < nearestDistance) {
                nearestDistance = distance;
                nearestFoodIndex = static_cast<int>(i);
            }
        }

        if (nearestFoodIndex >= 0 && critter.energy < 90.0f) {
            const Food& targetFood = foods[static_cast<std::size_t>(nearestFoodIndex)];
            float dx = targetFood.x - critter.x;
            float dz = targetFood.z - critter.z;
            critter.directionX = dx;
            critter.directionZ = dz;
            NormalizeDirection(critter.directionX, critter.directionZ);
        } else {
            if (critter.wanderTimer <= 0.0f || (std::abs(critter.directionX) < 0.01f && std::abs(critter.directionZ) < 0.01f)) {
                critter.directionX = RandomRange(-1.0f, 1.0f);
                critter.directionZ = RandomRange(-1.0f, 1.0f);
                NormalizeDirection(critter.directionX, critter.directionZ);
                critter.wanderTimer = RandomRange(0.5f, 2.5f);
            }
            critter.wanderTimer -= deltaTime;
        }

        float speed = critter.genes.speed * deltaTime * 3.0f;
        critter.x += critter.directionX * speed;
        critter.z += critter.directionZ * speed;
        critter.y = 0.0f;

        critter.x = Clamp(critter.x, -WORLD_HALF_SIZE, WORLD_HALF_SIZE);
        critter.z = Clamp(critter.z, -WORLD_HALF_SIZE, WORLD_HALF_SIZE);
        critter.energy -= deltaTime * (1.0f + critter.genes.speed * 0.25f);

        for (auto foodIt = foods.begin(); foodIt != foods.end(); ++foodIt) {
            float dx = foodIt->x - critter.x;
            float dz = foodIt->z - critter.z;
            float distance = std::sqrt(dx * dx + dz * dz);

            if (distance < 2.0f) {
                critter.energy += 40.0f;
                foodIt = foods.erase(foodIt);
                break;
            }
        }

        critter.energy = Clamp(critter.energy, 0.0f, 200.0f);
    }

    for (std::size_t i = 0; i < critters.size(); ) {
        Critter& critter = critters[i];
        if (critter.energy > 150.0f && critters.size() < MAX_CRITTERS && (rand() % 100) < 25) {
            Critter child = critter;
            child.energy = critter.energy * 0.4f;
            critter.energy *= 0.6f;

            child.x += RandomRange(-2.0f, 2.0f);
            child.z += RandomRange(-2.0f, 2.0f);
            child.x = Clamp(child.x, -WORLD_HALF_SIZE, WORLD_HALF_SIZE);
            child.z = Clamp(child.z, -WORLD_HALF_SIZE, WORLD_HALF_SIZE);

            child.genes.speed = Clamp(child.genes.speed + RandomRange(-1.2f, 1.2f), 2.0f, 12.0f);
            child.genes.sightRadius = Clamp(child.genes.sightRadius + RandomRange(-4.0f, 4.0f), 8.0f, 36.0f);
            child.genes.r = Clamp(child.genes.r + RandomRange(-0.25f, 0.25f), 0.0f, 1.0f);
            child.genes.g = Clamp(child.genes.g + RandomRange(-0.25f, 0.25f), 0.0f, 1.0f);
            child.genes.b = Clamp(child.genes.b + RandomRange(-0.25f, 0.25f), 0.0f, 1.0f);

            critters.push_back(child);
        }
        ++i;
    }

    if (foodTimer <= 0.0f) {
        foodTimer = FOOD_REGEN_INTERVAL;
        if (foods.size() < MAX_FOOD) {
            Food food;
            food.x = RandomRange(-WORLD_HALF_SIZE, WORLD_HALF_SIZE);
            food.z = RandomRange(-WORLD_HALF_SIZE, WORLD_HALF_SIZE);
            food.y = 0.0f;
            foods.push_back(food);
        }
    }
}