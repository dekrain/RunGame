#include "run.hpp"

#include <cstring>
#include <cmath>
#include <cstdio>

#include <GL/glew.h>

inline const double C_PI = std::acos(-1);

LevelInfo LoadLevelFromArray(size_t num_sectors, uint8_t* data) {
    LevelInfo info {
        .num_sectors = num_sectors,
        .sector_data = std::vector<GeometrySector>(num_sectors)
    };
    memcpy(info.sector_data.data(), data, num_sectors * num_slots * sizeof(uint8_t));
    return info;
}

static Col sColorMap[] = {
    {0., 0., 0.}, // empty (skipped)
    {1., 1., 1.}, // present (normal)
    {.2, .2, .2}, // selected (empty)
    {0., 1., 0.}, // selected (present)
};

void GenerateLevelSceneModel(LevelInfo const& level, uint32_t vbo, size_t& num_vtx) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    auto meshbuf = std::unique_ptr<float[]>(new float[2 * 18 * level.num_sectors * num_slots]);
    float* meshptr = meshbuf.get(); // current XYZ vertex of mesh
    uint8_t const* cursor = reinterpret_cast<uint8_t const*>(level.sector_data.data());

    for (size_t z = 0; z < level.num_sectors; ++z) {
        // Generate mesh of quads
        // First floor must be flat horizontal, so phase offset is phi/2
        // where phi = 2pi/num_floors
        const double phi = 2*C_PI / num_floors;
        for (uint32_t i = 0; i < num_floors; ++i) {
            const double angle = i*phi;
            float xl, yl, xr, yr; // XY pos of left/right corners
            xl =  std::sin(angle - phi/2);
            yl = -std::cos(angle - phi/2);
            xr =  std::sin(angle + phi/2);
            yr = -std::cos(angle + phi/2);

            for (uint32_t j = 0; j < num_floor_planes; ++j) {
                uint8_t index = *cursor++;
                if (index == 0)
                    continue;
                Col color = sColorMap[index];
                // Interpolate the vertices
                // XY = lerp(XY0, XY1, j/num_floor_planes)
                float xp0, yp0, xp1, yp1;
                const float j0 = j, j1 = j + 1;
                xp0 = xl + (j0/num_floor_planes) * (xr - xl);
                yp0 = yl + (j0/num_floor_planes) * (yr - yl);
                xp1 = xl + (j1/num_floor_planes) * (xr - xl);
                yp1 = yl + (j1/num_floor_planes) * (yr - yl);

                // Append quad to the mesh (for now without element buffer; 36 floats per plane)
                #define SET_COLOR \
                *meshptr++ = color[0];\
                *meshptr++ = color[1];\
                *meshptr++ = color[2];

                // Triangle 1
                *meshptr++ = xp0;
                *meshptr++ = yp0;
                *meshptr++ = -(float)z;
                SET_COLOR

                *meshptr++ = xp1;
                *meshptr++ = yp1;
                *meshptr++ = -(float)z;
                SET_COLOR

                *meshptr++ = xp1;
                *meshptr++ = yp1;
                *meshptr++ = -(float)z-1.f;
                SET_COLOR

                // Triangle 2
                *meshptr++ = xp1;
                *meshptr++ = yp1;
                *meshptr++ = -(float)z-1.f;
                SET_COLOR

                *meshptr++ = xp0;
                *meshptr++ = yp0;
                *meshptr++ = -(float)z-1.f;
                SET_COLOR

                *meshptr++ = xp0;
                *meshptr++ = yp0;
                *meshptr++ = -(float)z;
                SET_COLOR
            }
        }
    }

    // Upload mesh data
    num_vtx = (meshptr - meshbuf.get()) / (2 * 3);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * num_vtx * 2 * 3, meshbuf.get(), GL_DYNAMIC_DRAW);
}

void GetFloorProperties(
    float& yval,
    float& pwidth,
    float& xmin,
    float& xmax
) {
    const double phi = 2*C_PI / num_floors;
    yval = -std::cos(phi/2);
    xmax =  std::sin(phi/2);
    xmin = -xmax;
    pwidth = 2*xmax / num_floor_planes;
}

void GenerateCharacterModel(uint32_t vbo) {
    static Vtx sModel[] = {
        {{-.5f, 0.f, 0.f}, {1.f, .2f, 0.f}},
        {{ .5f, 0.f, 0.f}, {1.f, .2f, 0.f}},
        {{-.5f, 1.f, 0.f}, {1.f, .2f, 0.f}},
        {{ .5f, 1.f, 0.f}, {1.f, .2f, 0.f}},
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sModel), sModel, GL_STATIC_DRAW);
}

void DumpLevelToFile(LevelInfo const& level, char const* fname) {
    std::FILE* file = std::fopen(fname, "wb");
    uint32_t hdr = (level.num_sectors & 0xFFFF) | ((num_floor_planes & 0xFF) << 0x10) | ((num_floors & 0xFF) << 0x18);
    std::fwrite(&hdr, sizeof(uint32_t), 1, file);
    for (size_t i = 0; i < level.num_sectors; ++i) {
        GeometrySector const& sec = level.sector_data[i];
        unsigned char buf[(num_slots + 7) / 8]; // Pack the slots in bitarray, padded with 0s if necessary
        memset(buf, 0, sizeof(buf));
        for (size_t s = 0; s < num_slots; ++s)
            buf[s / 8] |= static_cast<bool>(sec.data[s] & 1) << (s & 7);
        std::fwrite(buf, 1, sizeof(buf), file);
    }
    std::fclose(file);
}

LevelInfo LoadLevelFromFile(char const* fname) {
    std::FILE* file = std::fopen(fname, "rb");
    uint32_t hdr;
    std::fread(&hdr, sizeof(uint32_t), 1, file);
    LevelInfo level {
        .num_sectors = hdr & 0xFFFF,
        .sector_data = std::vector<GeometrySector>(hdr & 0xFFFF)
    };
    for (size_t i = 0; i < level.num_sectors; ++i) {
        GeometrySector& sec = level.sector_data[i];
        unsigned char buf[(num_slots + 7) / 8];
        std::fread(buf, 1, sizeof(buf), file);
        for (size_t s = 0; s < num_slots; ++s)
            sec.data[s] = (buf[s / 8] >> (s & 7)) & 1;
    }
    std::fclose(file);
    return level;
}
