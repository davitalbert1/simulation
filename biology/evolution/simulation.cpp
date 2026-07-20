#include "simulation.h"

// --- ESTADO GLOBAL DA SIMULAÇÃO (Definição) ---
std::vector<Critter> critters;
std::vector<Food> foods;

void ResetSimulation() {
    critters.clear();
    foods.clear();
    // TODO: Initialize or reset the state of all simulation objects
}

void UpdateSimulation(float deltaTime) {
    // TODO: Update the state of all simulation objects based on deltaTime
}