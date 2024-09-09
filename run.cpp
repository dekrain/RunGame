#include "run.hpp"

#include <cstring>
#include <cmath>
#include <cstdio>

#include <GL/glew.h>

inline double const C_PI = std::acos(-1);

GeometrySegment::~GeometrySegment() {
    glDeleteVertexArrays(1, &gl_vao);
    glDeleteBuffers(1, &gl_vbo);
}

LevelInfo LoadBlankLevel() {
    LevelInfo info;

    GeometrySegment& seg = *info.segments.emplace_back(new GeometrySegment);
    // 5 planes per 4 floors per sector
    seg.floors = 4;
    seg.floor_planes = 5;
    seg.sectors = 3;
    seg.data.resize(seg.floors * seg.floor_planes * seg.sectors, 1);
    GetFloorProperties(seg);
    return info;
}

void CleanupLevel(LevelInfo& level) {
    level.segments.clear();
    level.segments.shrink_to_fit();
}

static Col sColorMap[] = {
    {0., 0., 0.}, // empty (skipped)
    {1., 1., 1.}, // present (normal)
    {.2, .2, .2}, // selected (empty)
    {0., 1., 0.}, // selected (present)
};

void GenerateLevelSceneModel(GeometrySegment& seg) {
    glBindBuffer(GL_ARRAY_BUFFER, seg.gl_vbo);

    auto meshbuf = std::unique_ptr<float[]>(new float[2 * 18 * seg.sectors * seg.floors * seg.floor_planes]);
    float* meshptr = meshbuf.get(); // current XYZ vertex of mesh
    uint8_t const* cursor = seg.data.data();

    for (size_t z = 0; z < seg.sectors; ++z) {
        // Generate mesh of quads
        // First floor must be flat horizontal, so phase offset is phi/2
        // where phi = 2pi/num_floors
        double const phi = 2*C_PI / seg.floors;
        for (uint32_t i = 0; i < seg.floors; ++i) {
            double const angle = i*phi;
            float xl, yl, xr, yr; // XY pos of left/right corners
            xl =  std::sin(angle - phi/2);
            yl = -std::cos(angle - phi/2);
            xr =  std::sin(angle + phi/2);
            yr = -std::cos(angle + phi/2);

            for (uint32_t j = 0; j < seg.floor_planes; ++j) {
                uint8_t index = *cursor++;
                if (index == 0)
                    continue;
                Col color = sColorMap[index];
                // Interpolate the vertices
                // XY = lerp(XY0, XY1, j/num_floor_planes)
                float xp0, yp0, xp1, yp1;
                float const j0 = j, j1 = j + 1;
                xp0 = xl + (j0/seg.floor_planes) * (xr - xl);
                yp0 = yl + (j0/seg.floor_planes) * (yr - yl);
                xp1 = xl + (j1/seg.floor_planes) * (xr - xl);
                yp1 = yl + (j1/seg.floor_planes) * (yr - yl);

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
                #undef SET_COLOR
            }
        }
    }

    // Upload mesh data
    seg.vtx_count = (meshptr - meshbuf.get()) / (2 * 3);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * seg.vtx_count * 2 * 3, meshbuf.get(), GL_DYNAMIC_DRAW);
}

void GenerateSegmentSelectionModel(GeometrySegment const& seg) {
    size_t const vtx_count = 6 * seg.floors;
    auto meshbuf = std::unique_ptr<float[]>(new float[vtx_count * 2 * 3]);
    float* meshptr = meshbuf.get();

    double const phi = 2*C_PI / seg.floors;
    for (uint32_t floor = 0; floor != seg.floors; ++floor) {
        double const angle = floor*phi;
        float xl, yl, xr, yr; // XY pos of left/right corners
        xl =  std::sin(angle - phi/2);
        yl = -std::cos(angle - phi/2);
        xr =  std::sin(angle + phi/2);
        yr = -std::cos(angle + phi/2);

        // Triangle 1
        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = 0.f;
        *meshptr++ = 0.f;
        *meshptr++ = 1.f;
        *meshptr++ = 0.f;

        *meshptr++ = xr;
        *meshptr++ = yr;
        *meshptr++ = 0.f;
        *meshptr++ = 0.f;
        *meshptr++ = 1.f;
        *meshptr++ = 0.f;

        *meshptr++ = xr;
        *meshptr++ = yr;
        *meshptr++ = -1.f;
        *meshptr++ = 0.f;
        *meshptr++ = 1.f;
        *meshptr++ = 0.f;

        // Triangle 1
        *meshptr++ = xr;
        *meshptr++ = yr;
        *meshptr++ = -1.f;
        *meshptr++ = 0.f;
        *meshptr++ = 1.f;
        *meshptr++ = 0.f;

        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = -1.f;
        *meshptr++ = 0.f;
        *meshptr++ = 1.f;
        *meshptr++ = 0.f;

        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = 0.f;
        *meshptr++ = 0.f;
        *meshptr++ = 1.f;
        *meshptr++ = 0.f;
    }

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vtx_count * 2 * 3, meshbuf.get(), GL_DYNAMIC_DRAW);
}

void GetFloorProperties(GeometrySegment& seg) {
    double const phi = C_PI / seg.floors;
    seg.yval = -std::cos(phi);
    seg.xmax =  std::sin(phi);
    seg.xmin = -seg.xmax;
    seg.pwidth = 2*seg.xmax / seg.floor_planes;
}

void GenerateCharacterModel(uint32_t vbo) {
    constexpr Col cPlayerColor = {1.f, .2f, 0.f};
    static Vtx sModel[] = {
        {{-.5f, 0.f, 0.f}, cPlayerColor},
        {{ .5f, 0.f, 0.f}, cPlayerColor},
        {{-.5f, 1.f, 0.f}, cPlayerColor},
        {{ .5f, 1.f, 0.f}, cPlayerColor},
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sModel), sModel, GL_STATIC_DRAW);
}

void SetupSegmentBuffers(GeometrySegment& seg) {
    glGenVertexArrays(1, &seg.gl_vao);
    glGenBuffers(1, &seg.gl_vbo);
    glBindVertexArray(seg.gl_vao);
    SetupLevelMeshArray(seg.gl_vbo);
}

void SetupLevelMeshArray(uint32_t vbo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, col)));
    glEnableVertexAttribArray(1);
}

void DumpLevelToFile(LevelInfo const& level, char const* fname) {
    std::FILE* file = std::fopen(fname, "wb");
    uint32_t const lv_hdr = (leveldata_version << 0x10) | (level.segments.size() & 0xFFFF);
    std::fwrite(&lv_hdr, sizeof(uint32_t), 1, file);
    for (auto& seg_ : level.segments) {
        GeometrySegment const& seg = *seg_;
        uint32_t const seg_hdr =
            (seg.sectors & 0xFFFF) | ((seg.floor_planes & 0xFF) << 0x10) | ((seg.floors & 0xFF) << 0x18);
        std::fwrite(&seg_hdr, sizeof(uint32_t), 1, file);
        size_t const num_slots = seg.floors * seg.floor_planes * seg.sectors;
        size_t const buf_size = (num_slots + 7) / 8; // Pack the slots in bitarray, padded with 0s if necessary
        uint8_t* buf = reinterpret_cast<uint8_t*>(alloca(buf_size));
        memset(buf, 0, buf_size);
        for (size_t s = 0; s < num_slots; ++s)
            buf[s / 8] |= static_cast<bool>(seg.data[s] & 1) << (s & 7);
        std::fwrite(buf, 1, buf_size, file);
    }
    std::fclose(file);
}

bool LoadLevelFromFile(LevelInfo& level, char const* fname) {
    std::FILE* file = std::fopen(fname, "rb");
    if (!file) {
        std::perror("Could not load level");
        return false;
    }
    uint32_t lv_hdr;
    std::fread(&lv_hdr, sizeof(uint32_t), 1, file);
    if (lv_hdr >> 0x10 != leveldata_version) {
        std::fprintf(stderr, "Warning: Level's data version (%hu) does not match game data version (%hu)\n",
            static_cast<uint16_t>(lv_hdr >> 0x10), leveldata_version);
    }
    CleanupLevel(level);
    size_t nr_segments = lv_hdr & 0xFFFF;
    level.segments.reserve(nr_segments);
    while (nr_segments --) {
        uint32_t seg_hdr;
        std::fread(&seg_hdr, sizeof(uint32_t), 1, file);
        auto& seg = *level.segments.emplace_back(new GeometrySegment);
        seg.floors = (seg_hdr >> 0x18) & 0xFF;
        seg.floor_planes = (seg_hdr >> 0x10) & 0xFF;
        seg.sectors = seg_hdr & 0xFFFF;
        size_t const num_slots = seg.floors * seg.floor_planes * seg.sectors;
        size_t const buf_size = (num_slots + 7) / 8;
        uint8_t* buf = reinterpret_cast<uint8_t*>(alloca(buf_size));
        std::fread(buf, 1, buf_size, file);
        seg.data.resize(num_slots);
        for (size_t s = 0; s < num_slots; ++s)
            seg.data[s] = (buf[s / 8] >> (s & 7)) & 1;
        GetFloorProperties(seg);
        SetupSegmentBuffers(seg);
        GenerateLevelSceneModel(seg);
    }
    std::fclose(file);
    return true;
}
