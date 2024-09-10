#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <vector>

#include "event.hpp"

constexpr uint16_t leveldata_version = 2;

// 1 means floor plane present
// 0 means empty space

// Each segment can have different floor/plane configuration
struct GeometrySegment {
    uint32_t floors; // Number of standable floors; may be 0 for empty space (gap segment)
    uint32_t floor_planes; // Number of divisions (tiles) per floor
    uint32_t sectors; // Number of level sectors (tiles along Z axis)

    // Floor data
    float yval;
    float pwidth;
    float xmin;
    float xmax;

    // Segment data
    std::vector<uint8_t> data;
    uint32_t gl_vao = 0;
    uint32_t gl_vbo = 0;
    size_t vtx_count;

    GeometrySegment() = default;
    ~GeometrySegment();

    GeometrySegment(GeometrySegment&&) = delete;
    GeometrySegment(GeometrySegment const&) = delete;
};

struct LevelInfo {
    std::vector<std::unique_ptr<GeometrySegment>> segments;
};

LevelInfo LoadBlankLevel(); // Default level on editor startup
LevelInfo LoadLevelFromArray(size_t num_sectors, uint8_t* data);
void CleanupLevel(LevelInfo& level);

/// Layout is array of Vtx
/// Sector-0 is at Z=0
/// Sector-n is at Z=-n (increment is Z += -1 for each next sector)
/// All XY coords are inside the unit circle (radius 1)
/// Order of floors/planes counter-clockwise
void GenerateLevelSceneModel(GeometrySegment& seg);
void GenerateCharacterModel(uint32_t vbo);
void SetupSegmentBuffers(GeometrySegment& seg);
void SetupLevelMeshArray(uint32_t vbo);

enum class MeshVisualMode : uint8_t {
    None,
    Outline,
    SectorWire,
    SlotWire,
};

void GenerateSegmentSelectionModel(GeometrySegment const& seg);
void GenerateSegmentOutlineModel(GeometrySegment const& seg);
void GenerateSegmentSectorWireModel(GeometrySegment const& seg);
void GenerateSegmentSlotWireModel(GeometrySegment const& seg);

// Gets geometric properties of the main floor (on the bottom)
void GetFloorProperties(GeometrySegment& seg);

//void RenderLevel(LevelInfo const& level);
//void RenderLevelWithSegment(LevelInfo const& level, uint32_t segment, uint32_t gl_vao);

void DumpLevelToFile(LevelInfo const& level, char const* fname);
// Returns true on success
bool LoadLevelFromFile(LevelInfo& level, char const* fname);


using Pos = std::array<float, 3>;
using Col = std::array<float, 3>;

struct Vtx {
    Pos pos;
    Col col;
};

struct GameStateDef {
    void (*init)(void* common_ctx, void* state_ctx);
    void (*handle_event)(WinEvent const& ev, void* ctx);
    void (*render)(void* ctx);
    void (*change)(void* ctx); // Action that happens when a different state is selected
};
