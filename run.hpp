#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <vector>

#include <SDL2/SDL.h>

constexpr uint32_t num_floors = 4; // Number of standable floors
constexpr uint32_t num_floor_planes = 5; // Number of divisions per floor
constexpr uint32_t num_slots = num_floors * num_floor_planes; // Number of geometry planes per sector

// 1 means floor plane present
// 0 means empty space

struct GeometrySector {
    std::array<uint8_t, num_slots> data;
};

struct LevelInfo {
    size_t num_sectors;
    std::vector<GeometrySector> sector_data;
};

LevelInfo LoadLevelFromArray(size_t num_sectors, uint8_t* data);

/// Layout is array of Vtx
/// Sector-0 is at Z=0
/// Sector-n is at Z=-n (increment is Z += -1 for each next sector)
/// All XY coords are inside the unit circle (radius 1)
/// Order of floors/planes counter-clockwise
void GenerateLevelSceneModel(LevelInfo const& level, uint32_t vbo, /*out*/ size_t& num_vtx);
void GenerateCharacterModel(uint32_t vbo);

// Gets geometric properties of the main floor (on the bottom)
void GetFloorProperties(
    float& yval,
    float& pwidth,
    float& xmin,
    float& xmax
);

void DumpLevelToFile(LevelInfo const& level, char const* fname);
LevelInfo LoadLevelFromFile(char const* fname);


using Pos = std::array<float, 3>;
using Col = std::array<float, 3>;

struct Vtx {
    Pos pos;
    Col col;
};

struct GameStateDef {
    void (*init)(void* common_ctx, void* state_ctx);
    void (*handle_event)(SDL_Event& ev, void* ctx);
    void (*render)(void* ctx);
    void (*change)(void* ctx); // Action that happens when a different state is selected
};
