#pragma once
#include "optics.h"

//  Aba simulacao: bancada optica 3D com vidros e fontes de luz moveis, mais o
//  tracado dos raios refratados.
void SimulationTabDraw();
void SimulationTabHandleKey(int vk);
void SimulationTabRenderHUD(int mx, int my, bool clicked, bool down);

struct Camera;
void SimulationTabSetCameraRef(Camera* cam);
void SimulationTabSetHelpVisible(bool visible);
bool SimulationTabHelpVisible();