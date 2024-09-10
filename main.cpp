#include <cstdio>
#include <cstdint>
#include <array>
#include <cmath>

#include <GL/glew.h>

#include "event.hpp"
#include "util.hpp"
#include "run.hpp"
#include "wnd.hpp"

/*static Vtx sPolygonData[] = {
    {{ .0f,  .5f, -.1f}},
    {{ .8f, -.5f,  0.f}},
    {{-.5f, -.5f,  0.f}}
};*/

// Z scaling of the level model
static float const sLevelZScale = 0.04f;

uint32_t LoadShaderFromFile(char const* fname, GLenum type) {
    auto src = ReadFile(fname, false);
    uint32_t shader = glCreateShader(type);

    char const* src_var = reinterpret_cast<char*>(src.data.get());
    int32_t size = src.size;
    glShaderSource(shader, 1, &src_var, &size);
    glCompileShader(shader);

    int32_t status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (!status) {
        int32_t log_size;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);

        char* buf = reinterpret_cast<char*>(alloca(log_size));
        glGetShaderInfoLog(shader, log_size, nullptr, buf);

        std::printf("Error while compiling shader: %s\n", buf);
    }

    return shader;
}

struct BasicShader {
    uint32_t prog;

    int32_t loc_uScale;
    int32_t loc_uDisplacement;
};

struct CommonState {
    LevelInfo level;
    BasicShader shader;
};

enum class SegmentMode : uint8_t {
    Tile, // Operate on individual tiles
    Sector, // Operate on a sector/Z-strip (?)
    Segment, // Operate on the whole segment
};

enum class SegmentBufferMode : uint8_t {
    Solid,
    Outline,
    SectorWire,
    SlotWire,
};

struct EditorState {
    CommonState* common;
    uint32_t cur_segment, cur_sector, cur_spot;
    float curZ;
    SegmentMode segment_mode;
    SegmentBufferMode segment_block_mode;
    MeshVisualMode segment_visual_mode;
    uint32_t segment_block_buffer;
    uint32_t segment_block_vao;
    uint32_t segment_block_floors;
};

struct PlayingState {
    CommonState* common;
    uint32_t player_vao, player_vbo;
    float curZ, speed;
};

static void editor_init(void* common_ctx, void* ctx) {
    CommonState& s_common = *reinterpret_cast<CommonState*>(common_ctx);
    EditorState& s_ctx = *reinterpret_cast<EditorState*>(ctx);

    s_ctx.common = &s_common;
    s_ctx.cur_segment = s_ctx.cur_sector = s_ctx.cur_spot = 0;
    s_ctx.segment_mode = SegmentMode::Tile;
    glGenBuffers(1, &s_ctx.segment_block_buffer);
    glGenVertexArrays(1, &s_ctx.segment_block_vao);

    glBindVertexArray(s_ctx.segment_block_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_ctx.segment_block_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, col)));
    glEnableVertexAttribArray(1);

    s_ctx.curZ = 0.0f;
    s_ctx.segment_block_floors = 0;
    s_ctx.segment_block_mode = SegmentBufferMode::Solid;
    s_ctx.segment_visual_mode = MeshVisualMode::Outline;
}

static void ToggleSectorMark(GeometrySegment& seg, uint32_t sector, uint32_t num_slots) {
    for (uint32_t slot = 0; slot != num_slots; ++slot) {
        seg.data[sector * num_slots + slot] ^= 2;
    }
}

static void editor_input(WinEvent const& ev, void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    LevelInfo& level = state.common->level;
    GeometrySegment& seg = *level.segments[state.cur_segment];
    uint32_t num_slots = seg.geo.floors * seg.geo.floor_planes;
    if (ev.type == EventType::KeyDown) {
        switch (ev.key.lkey) {
        case LogicalKey::W:
            state.curZ += 0.02f;
            break;
        case LogicalKey::S:
            state.curZ -= 0.02f;
            break;
        // mode change
        case LogicalKey::M: {
            switch (state.segment_mode) {
                case SegmentMode::Tile:
                    // Unmark the tile
                    seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
                    state.segment_mode = SegmentMode::Sector;
                    // Mark the sector
                    ToggleSectorMark(seg, state.cur_sector, num_slots);
                    // Regenerate scene
                    GenerateLevelSceneModel(seg);
                    break;
                case SegmentMode::Sector:
                    // Unmark the sector
                    ToggleSectorMark(seg, state.cur_sector, num_slots);
                    state.segment_mode = SegmentMode::Segment;
                    GenerateLevelSceneModel(seg);
                    break;
                case SegmentMode::Segment:
                    // Recycle the segment buffer
                    state.segment_mode = SegmentMode::Tile;
                    // Mark the tile
                    seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
                    GenerateLevelSceneModel(seg);
                    break;
            }
            break;
        }
        // select
        case LogicalKey::ArrowLeft: {
            if (state.segment_mode != SegmentMode::Tile)
                break;
            uint32_t new_spot = state.cur_spot;
            if (new_spot-- == 0) {
                new_spot = num_slots - 1;
            }
            // Mark the new spot
            seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
            seg.data[state.cur_sector * num_slots + new_spot] ^= 2;
            state.cur_spot = new_spot;
            // Regenerate segment
            GenerateLevelSceneModel(seg);
            break;
        }
        case LogicalKey::ArrowRight: {
            if (state.segment_mode != SegmentMode::Tile)
                break;
            uint32_t new_spot = state.cur_spot;
            if (++new_spot == num_slots) {
                new_spot = 0;
            }
            // Mark the new spot
            seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
            seg.data[state.cur_sector * num_slots + new_spot] ^= 2;
            state.cur_spot = new_spot;
            // Regenerate scene
            GenerateLevelSceneModel(seg);
            break;
        }
        case LogicalKey::ArrowDown: {
            if (state.segment_mode == SegmentMode::Segment) {
                if (state.cur_segment != 0) {
                    --state.cur_segment;
                    state.cur_spot = 0;
                    state.cur_sector = 0;
                }
                break;
            }
            uint32_t new_sector = state.cur_sector;
            if (new_sector-- != 0) {
                // Mark the new spot
                if (state.segment_mode == SegmentMode::Tile) {
                    seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
                    seg.data[new_sector * num_slots + state.cur_spot] ^= 2;
                } else {
                    ToggleSectorMark(seg, state.cur_sector, num_slots);
                    ToggleSectorMark(seg, new_sector, num_slots);
                }
                state.cur_sector = new_sector;
                // Regenerate scene
                GenerateLevelSceneModel(seg);
            } // TODO: Move between segments
            break;
        }
        case LogicalKey::ArrowUp: {
            if (state.segment_mode == SegmentMode::Segment) {
                if (state.cur_segment + 1 < level.segments.size()) {
                    ++state.cur_segment;
                    state.cur_spot = 0;
                    state.cur_sector = 0;
                }
                break;
            }
            uint32_t new_sector = state.cur_sector;
            if (++new_sector >= seg.geo.sectors) {
                // Temporarily disable the ability to make new sectors just by navigation
                break;
                // TODO: Move between segments if present
                //seg.data.resize(seg.data.size() + num_slots);
                //++seg.geo.sectors;
            }
            // Mark the new spot
            if (state.segment_mode == SegmentMode::Tile) {
                seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
                seg.data[new_sector * num_slots + state.cur_spot] ^= 2;
            } else {
                ToggleSectorMark(seg, state.cur_sector, num_slots);
                ToggleSectorMark(seg, new_sector, num_slots);
            }
            state.cur_sector = new_sector;
            // Regenerate scene
            GenerateLevelSceneModel(seg);
            break;
        }
        case LogicalKey::Space: {
            if (state.segment_mode != SegmentMode::Tile)
                break;
            // Set/reset the spot
            seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 1;
            // Regenerate scene
            GenerateLevelSceneModel(seg);
            break;
        }
        case LogicalKey::P: {
            DumpLevelToFile(state.common->level, "level.dat");
            std::puts("Dumped level to file 'level.dat'");
            break;
        }
        case LogicalKey::L: {
            if (LoadLevelFromFile(level, "level.dat")) {
                state.cur_segment = 0;
                state.cur_sector = 0;
                state.cur_spot = 0;
                state.segment_mode = SegmentMode::Tile;
                level.segments[0]->data[0] ^= 2;
                GenerateLevelSceneModel(*level.segments[0]);
                std::puts("Loaded level 'level.dat'");
            }
            break;
        }
        case LogicalKey::PageUp:
        case LogicalKey::Insert: {
            bool after = ev.key.lkey == LogicalKey::PageUp;
            // Insert section/sector
            switch (state.segment_mode) {
            case SegmentMode::Tile: break;
            case SegmentMode::Sector:
                ToggleSectorMark(seg, state.cur_sector, num_slots);
                seg.geo.sectors += 1;
                if (after) {
                    state.cur_sector += 1;
                }
                seg.data.insert(seg.data.begin() + state.cur_sector * num_slots, num_slots, 0);
                ToggleSectorMark(seg, state.cur_sector, num_slots);
                GenerateLevelSceneModel(seg);
                break;
            case SegmentMode::Segment: {
                if (after) {
                    state.cur_segment += 1;
                }
                GeometrySegment& newseg = **level.segments.emplace(level.segments.begin() + state.cur_segment, new GeometrySegment);
                // Copy geometry from current segment
                newseg.geo.floors = seg.geo.floors;
                newseg.geo.floor_planes = seg.geo.floor_planes;
                newseg.geo.sectors = 1;
                newseg.data.resize(newseg.geo.floors * newseg.geo.floor_planes * newseg.geo.sectors, 0);
                state.cur_sector = 0;
                GetFloorProperties(newseg);
                SetupSegmentBuffers(newseg);
                GenerateLevelSceneModel(newseg);
                break;
            }
            }
            break;
        }
        case LogicalKey::PageDown:
        case LogicalKey::Delete: {
            bool back = ev.key.lkey == LogicalKey::PageDown;
            // Delete section/sector
            switch (state.segment_mode) {
                case SegmentMode::Tile: break;
                case SegmentMode::Sector:
                    if (seg.geo.sectors <= 1)
                        break;
                    // No need to unmark, since it's getting deleted
                    seg.data.erase(seg.data.begin() + state.cur_sector * num_slots, seg.data.begin() + (state.cur_sector + 1) * num_slots);
                    seg.geo.sectors -= 1;
                    if (state.cur_sector == seg.geo.sectors or (back and state.cur_sector > 0)) {
                        state.cur_sector -= 1;
                    }
                    ToggleSectorMark(seg, state.cur_sector, num_slots);
                    GenerateLevelSceneModel(seg);
                    break;
                case SegmentMode::Segment:
                    if (level.segments.size() <= 1)
                        break;
                    level.segments.erase(level.segments.begin() + state.cur_segment);
                    if (state.cur_segment == level.segments.size() or (back and state.cur_segment > 0)) {
                        state.cur_segment -= 1;
                    }
                    break;
            }
            break;
        }
        case LogicalKey::Plus: {
            // Increase floor count
            break;
        }
        case LogicalKey::Minus: {
            // Decrease floor count
            break;
        }
        default: break;
        }
    }
}

void RenderLevel(CommonState const& common, float curZ) {
    glUniform3f(common.shader.loc_uScale, 1.f, 1.f, sLevelZScale);
    for (auto& seg : common.level.segments) {
        glUniform3f(common.shader.loc_uDisplacement, 0.f, 0.f, curZ);
        glBindVertexArray(seg->gl_vao);
        glDrawArrays(GL_TRIANGLES, 0, seg->vtx_count);
        curZ -= sLevelZScale * seg->geo.sectors;
    }
}

void RenderLevelWithSegment(CommonState const& common, uint32_t segment, uint32_t gl_vao, float curZ) {
    glUniform3f(common.shader.loc_uScale, 1.f, 1.f, sLevelZScale);
    for (uint32_t idx = 0; idx != common.level.segments.size(); ++idx) {
        auto const &seg = *common.level.segments[idx];
        glUniform3f(common.shader.loc_uDisplacement, 0.f, 0.f, curZ);
        if (idx == segment) {
            glBindVertexArray(gl_vao);
            glUniform3f(common.shader.loc_uScale, 1.f, 1.f, sLevelZScale * seg.geo.sectors);
            glDrawArrays(GL_TRIANGLES, 0, 6 * seg.geo.floors);
            glUniform3f(common.shader.loc_uScale, 1.f, 1.f, sLevelZScale);
        } else {
            glBindVertexArray(seg.gl_vao);
            glDrawArrays(GL_TRIANGLES, 0, seg.vtx_count);
        }
        curZ -= sLevelZScale * seg.geo.sectors;
    }
}

static void editor_render(void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(state.common->shader.prog);
    if (state.segment_mode == SegmentMode::Segment) {
        SegmentGeometry const& seg = state.common->level.segments[state.cur_segment]->geo;
        if (state.segment_block_floors != seg.floors or state.segment_block_mode != SegmentBufferMode::Solid) {
            glBindBuffer(GL_ARRAY_BUFFER, state.segment_block_buffer);
            GenerateSegmentSelectionModel(seg);
            state.segment_block_floors = seg.floors;
            state.segment_block_mode = SegmentBufferMode::Solid;
        }
        RenderLevelWithSegment(*state.common, state.cur_segment, state.segment_block_vao, state.curZ);
    } else {
        RenderLevel(*state.common, state.curZ);
        if (state.segment_visual_mode != MeshVisualMode::None) {
            auto const& level = state.common->level;
            glBindBuffer(GL_ARRAY_BUFFER, state.segment_block_buffer);
            SegmentGeometry const& seg = level.segments[state.cur_segment]->geo;
            bool regen = state.segment_block_floors != seg.floors;
            switch (state.segment_visual_mode) {
            case MeshVisualMode::None: break;
            case MeshVisualMode::Outline:
                regen |= state.segment_block_mode != SegmentBufferMode::Outline;
                if (regen) {
                    GenerateSegmentOutlineModel(seg);
                    state.segment_block_mode = SegmentBufferMode::Outline;
                    state.segment_block_floors = seg.floors;
                }
                break;
            case MeshVisualMode::SectorWire: break;
            case MeshVisualMode::SlotWire: break;
            }

            float curZ = state.curZ;
            for (auto it = level.segments.begin(), end = it + state.cur_segment; it != end; ++it) {
                curZ -= sLevelZScale * (**it).geo.sectors;
            }

            auto const& shader = state.common->shader;
            glBindVertexArray(state.segment_block_vao);
            glUniform3f(shader.loc_uDisplacement, 0.f, 0.f, curZ);
            glUniform3f(shader.loc_uScale, 1.f, 1.f, sLevelZScale * seg.sectors);
            //glUniform3f(shader.loc_uScale, 1.f, 1.f, sLevelZScale);
            glDrawArrays(GL_LINES, 0, 6 * seg.floors);
        }
    }
}

static void editor_switch(void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    GeometrySegment& seg = *state.common->level.segments[state.cur_segment];
    switch (state.segment_mode) {
        case SegmentMode::Tile:
            seg.data[state.cur_sector * seg.geo.floors * seg.geo.floor_planes + state.cur_spot] ^= 2;
            break;
        case SegmentMode::Sector:
            ToggleSectorMark(seg, state.cur_sector, seg.geo.floors * seg.geo.floor_planes);
            break;
        case SegmentMode::Segment:
            // Already clean
            break;
    }
    GenerateLevelSceneModel(seg);
}

static void game_init(void* common_ctx, void* ctx) {
    CommonState& s_common = *reinterpret_cast<CommonState*>(common_ctx);
    PlayingState& s_ctx = *reinterpret_cast<PlayingState*>(ctx);

    s_ctx.common = &s_common;

    glGenVertexArrays(1, &s_ctx.player_vao);
    glGenBuffers(1, &s_ctx.player_vbo);

    GenerateCharacterModel(s_ctx.player_vbo);

    glBindVertexArray(s_ctx.player_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_ctx.player_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, col)));
    glEnableVertexAttribArray(1);

    s_ctx.curZ = 0.0f;
    s_ctx.speed = 0.0f;
}

static void game_input(WinEvent const& ev, void* ctx) {}

static void game_render(void* ctx) {
    PlayingState& state = *reinterpret_cast<PlayingState*>(ctx);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static float const zfactor = -0.08f;

    state.curZ += state.speed;

    glUseProgram(state.common->shader.prog);
    RenderLevel(*state.common, state.curZ + zfactor);

    glBindVertexArray(state.player_vao);
    glUniform3f(state.common->shader.loc_uScale, state.common->level.segments[0]->pwidth, .4f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0., state.common->level.segments[0]->yval, zfactor);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void game_switch(void* ctx) {
    PlayingState& state = *reinterpret_cast<PlayingState*>(ctx);
    state.curZ = 0.0f;
    state.speed = 0.002f;
}

static void common_init(CommonState& state) {
    // Init scene
    state.level = LoadBlankLevel();
    SetupSegmentBuffers(*state.level.segments[0]);

    // Load level geometry
    GenerateLevelSceneModel(*state.level.segments[0]);

    // Load shaders
    uint32_t vs = LoadShaderFromFile("basic.vs.glsl", GL_VERTEX_SHADER);
    uint32_t fs = LoadShaderFromFile("basic.fs.glsl", GL_FRAGMENT_SHADER);

    uint32_t shdr = glCreateProgram();
    glAttachShader(shdr, vs);
    glAttachShader(shdr, fs);

    // Unnecessary
    //glBindFragDataLocation(shdr, 0, "fColor");
    glBindAttribLocation(shdr, 0, "vPos");
    glBindAttribLocation(shdr, 1, "vColor");

    glLinkProgram(shdr);
    state.shader.prog = shdr;
    state.shader.loc_uScale = glGetUniformLocation(shdr, "uScale");
    state.shader.loc_uDisplacement = glGetUniformLocation(shdr, "uDisplacement");
}

static void common_finish(CommonState& state, EditorState& s_editor, PlayingState& s_playing) {
    CleanupLevel(state.level);
    glDeleteVertexArrays(1, &s_playing.player_vao);
    glDeleteBuffers(1, &s_playing.player_vbo);
}

int main() {
    static GameStateDef state_def_editor {
        &editor_init,
        &editor_input,
        &editor_render,
        &editor_switch
    };
    static GameStateDef state_def_game {
        &game_init,
        &game_input,
        &game_render,
        &game_switch
    };

    CommonState s_common;
    EditorState s_editor;
    PlayingState s_game;

    GameStateDef const* state = &state_def_editor;
    void* state_ctx = &s_editor;

    WindowState* window;
    {
        InitParams params = {
            .gl_major = 3,
            .gl_minor = 2,
            .stencil_size = 8,
            .init_width = 800,
            .init_height = 800,
            .title = "Test ogl",
        };
        window = init_window(params);
        if (!window) {
            return 1;
        }
    }

    make_current(window);
    glewExperimental = true;
    glewInit();

    // Main loop
    common_init(s_common);
    editor_init(&s_common, &s_editor);
    game_init(&s_common, &s_game);

    state->change(state_ctx);

    WinEvent ev;
    while (true) {
        while (window_pop_event(window, ev)) {
            if (ev.type == EventType::Quit)
                goto end_prog;
            else if (ev.type == EventType::KeyDown && ev.key.lkey == LogicalKey::B) {
                state->change(state_ctx);
                if (state == &state_def_editor) {
                    state = &state_def_game;
                    state_ctx = &s_game;
                } else {
                    state = &state_def_editor;
                    state_ctx = &s_editor;
                }
                state->change(state_ctx);
            } else
                state->handle_event(ev, state_ctx);
        }

        // Render
        state->render(state_ctx);
        window_swap(window);
    }
    end_prog:

    // Deinit
    common_finish(s_common, s_editor, s_game);
    window_finish(window);
}
