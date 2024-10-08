#include "run.hpp"

//#include <cassert>
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
    seg.geo.floors = 4;
    seg.geo.floor_planes = 5;
    seg.geo.sectors = 3;
    seg.data.resize(seg.geo.floors * seg.geo.floor_planes * seg.geo.sectors, 1);
    GetFloorProperties(seg);
    return info;
}

void CleanupLevel(LevelInfo& level) {
    level.segments.clear();
    level.segments.shrink_to_fit();
}

static Col const sColorMap[] = {
    {0., 0., 0.}, // empty (skipped)
    {1., 1., 1.}, // present (normal)
    {.2, .2, .2}, // selected (empty)
    {0., 1., 0.}, // selected (present)
};

void GenerateLevelSceneModel(GeometrySegment& seg) {
    glBindBuffer(GL_ARRAY_BUFFER, seg.gl_vbo);

    auto meshbuf = std::unique_ptr<float[]>(new float[2 * 18 * seg.geo.sectors * seg.geo.floors * seg.geo.floor_planes]);
    float* meshptr = meshbuf.get(); // current XYZ vertex of mesh
    uint8_t const* cursor = seg.data.data();

    for (size_t z = 0; z < seg.geo.sectors; ++z) {
        // Generate mesh of quads
        // First floor must be flat horizontal, so phase offset is phi/2
        // where phi = 2pi/num_floors
        double const phi = 2*C_PI / seg.geo.floors;
        for (uint32_t i = 0; i < seg.geo.floors; ++i) {
            double const angle = i*phi;
            float xl, yl, xr, yr; // XY pos of left/right corners
            xl =  std::sin(angle - phi/2);
            yl = -std::cos(angle - phi/2);
            xr =  std::sin(angle + phi/2);
            yr = -std::cos(angle + phi/2);

            for (uint32_t j = 0; j < seg.geo.floor_planes; ++j) {
                uint8_t index = *cursor++;
                if (index == 0)
                    continue;
                Col color = sColorMap[index];
                // Interpolate the vertices
                // XY = lerp(XY0, XY1, j/num_floor_planes)
                float xp0, yp0, xp1, yp1;
                float const j0 = j, j1 = j + 1;
                xp0 = xl + (j0/seg.geo.floor_planes) * (xr - xl);
                yp0 = yl + (j0/seg.geo.floor_planes) * (yr - yl);
                xp1 = xl + (j1/seg.geo.floor_planes) * (xr - xl);
                yp1 = yl + (j1/seg.geo.floor_planes) * (yr - yl);

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

void GenerateSegmentSelectionModel(SegmentGeometry const& geo) {
    size_t const vtx_count = 6 * geo.floors;
    auto meshbuf = std::unique_ptr<float[]>(new float[vtx_count * 2 * 3]);
    float* meshptr = meshbuf.get();

    double const phi = 2*C_PI / geo.floors;
    for (uint32_t floor = 0; floor != geo.floors; ++floor) {
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

    //assert(static_cast<size_t>(meshptr - meshbuf.get()) == vtx_count * 2 * 3);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vtx_count * 2 * 3, meshbuf.get(), GL_DYNAMIC_DRAW);
}

static constexpr Col cMeshOutlineColor = {0.8, 0.4, 0.2};

#define SET_COLOR \
    *meshptr++ = cMeshOutlineColor[0]; \
    *meshptr++ = cMeshOutlineColor[1]; \
    *meshptr++ = cMeshOutlineColor[2];

void GenerateSegmentOutlineModel(SegmentGeometry const& geo) {
    size_t const line_count = 3 * geo.floors;

    auto meshbuf = std::unique_ptr<float[]>(new float[line_count * 2 * 3 * 2]);
    float* meshptr = meshbuf.get();

    double const phi = 2*C_PI / geo.floors;
    for (uint32_t floor = 0; floor != geo.floors; ++floor) {
        double const angle = floor*phi;
        float xl, yl, xr, yr; // XY pos of left/right corners
        xl =  std::sin(angle - phi/2);
        yl = -std::cos(angle - phi/2);
        xr =  std::sin(angle + phi/2);
        yr = -std::cos(angle + phi/2);

        // Front line
        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = 0.f;
        SET_COLOR

        *meshptr++ = xr;
        *meshptr++ = yr;
        *meshptr++ = 0.f;
        SET_COLOR

        // Back line
        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = -1.f;
        SET_COLOR

        *meshptr++ = xr;
        *meshptr++ = yr;
        *meshptr++ = -1.f;
        SET_COLOR

        // Side line
        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = 0.f;
        SET_COLOR

        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = -1.f;
        SET_COLOR
    }

    //assert(static_cast<size_t>(meshptr - meshbuf.get()) == line_count * 2 * 3 * 2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * line_count * 2 * 3 * 2, meshbuf.get(), GL_DYNAMIC_DRAW);
}

void GenerateSegmentSectorWireModel(SegmentGeometry const& geo) {
    size_t const line_count = (geo.sectors + 2) * geo.floors;

    auto meshbuf = std::unique_ptr<float[]>(new float[line_count * 2 * 3 * 2]);
    float* meshptr = meshbuf.get();

    double const phi = 2*C_PI / geo.floors;
    for (uint32_t floor = 0; floor != geo.floors; ++floor) {
        double const angle = floor*phi;
        float xl, yl, xr, yr; // XY pos of left/right corners
        xl =  std::sin(angle - phi/2);
        yl = -std::cos(angle - phi/2);
        xr =  std::sin(angle + phi/2);
        yr = -std::cos(angle + phi/2);

        // Side line
        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = 0.f;
        SET_COLOR

        *meshptr++ = xl;
        *meshptr++ = yl;
        *meshptr++ = -(float)geo.sectors;
        SET_COLOR

        // Sector lines (one more line at the last sector)
        for (uint32_t z = 0; z <= geo.sectors; ++z) {
            *meshptr++ = xl;
            *meshptr++ = yl;
            *meshptr++ = -(float)z;
            SET_COLOR

            *meshptr++ = xr;
            *meshptr++ = yr;
            *meshptr++ = -(float)z;
            SET_COLOR
        }
    }

    //assert(static_cast<size_t>(meshptr - meshbuf.get()) == line_count * 2 * 3 * 2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * line_count * 2 * 3 * 2, meshbuf.get(), GL_DYNAMIC_DRAW);
}

void GenerateSegmentSlotWireModel(SegmentGeometry const& geo) {
    size_t const line_count = (geo.sectors + 1 + geo.floor_planes) * geo.floors;

    auto meshbuf = std::unique_ptr<float[]>(new float[line_count * 2 * 3 * 2]);
    float* meshptr = meshbuf.get();

    double const phi = 2*C_PI / geo.floors;
    for (uint32_t floor = 0; floor != geo.floors; ++floor) {
        double const angle = floor*phi;
        float xl, yl, xr, yr; // XY pos of left/right corners
        xl =  std::sin(angle - phi/2);
        yl = -std::cos(angle - phi/2);
        xr =  std::sin(angle + phi/2);
        yr = -std::cos(angle + phi/2);

        // Side/floor plane lines
        for (uint32_t part = 0; part < geo.floor_planes; ++part) {
            // Interpolate the vertices
            // XY = lerp(XY0, XY1, j/num_floor_planes)
            float const p0 = part;
            float const xp = xl + (p0/geo.floor_planes) * (xr - xl);
            float const yp = yl + (p0/geo.floor_planes) * (yr - yl);

            *meshptr++ = xp;
            *meshptr++ = yp;
            *meshptr++ = 0.f;
            SET_COLOR

            *meshptr++ = xp;
            *meshptr++ = yp;
            *meshptr++ = -(float)geo.sectors;
            SET_COLOR
        }

        // Sector lines (one more line at the last sector)
        for (uint32_t z = 0; z <= geo.sectors; ++z) {
            *meshptr++ = xl;
            *meshptr++ = yl;
            *meshptr++ = -(float)z;
            SET_COLOR

            *meshptr++ = xr;
            *meshptr++ = yr;
            *meshptr++ = -(float)z;
            SET_COLOR
        }
    }

    //assert(static_cast<size_t>(meshptr - meshbuf.get()) == line_count * 2 * 3 * 2);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * line_count * 2 * 3 * 2, meshbuf.get(), GL_DYNAMIC_DRAW);
}

#undef SET_COLOR

void GetFloorProperties(GeometrySegment& seg) {
    double const phi = C_PI / seg.geo.floors;
    seg.yval = -std::cos(phi);
    seg.xmax =  std::sin(phi);
    seg.xmin = -seg.xmax;
    seg.pwidth = 2*seg.xmax / seg.geo.floor_planes;
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
            (seg.geo.sectors & 0xFFFF) | ((seg.geo.floor_planes & 0xFF) << 0x10) | ((seg.geo.floors & 0xFF) << 0x18);
        std::fwrite(&seg_hdr, sizeof(uint32_t), 1, file);
        size_t const num_slots = seg.geo.floors * seg.geo.floor_planes * seg.geo.sectors;
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
        seg.geo.floors = (seg_hdr >> 0x18) & 0xFF;
        seg.geo.floor_planes = (seg_hdr >> 0x10) & 0xFF;
        seg.geo.sectors = seg_hdr & 0xFFFF;
        size_t const num_slots = seg.geo.floors * seg.geo.floor_planes * seg.geo.sectors;
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
