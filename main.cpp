#include <cstdio>
#include <cstdint>
#include <array>
#include <cmath>

#include <GL/glew.h>

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

struct EditorState {
    CommonState* common;
    uint32_t cur_segment, cur_sector, cur_spot;
    float curZ;
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

    s_ctx.curZ = 0.0f;
}

static void editor_input(WinEvent const& ev, void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    LevelInfo& level = state.common->level;
    GeometrySegment& seg = level.segments[state.cur_segment];
    uint32_t num_slots = seg.floors * seg.floor_planes;
    if (ev.type == EventType::KeyDown) {
        if (ev.key.lkey == LogicalKey::W) {
            state.curZ += 0.02f;
        } else if (ev.key.lkey == LogicalKey::S) {
            state.curZ -= 0.02f;
        }
        // select
        else if (ev.key.lkey == LogicalKey::ArrowLeft) {
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
        } else if (ev.key.lkey == LogicalKey::ArrowRight) {
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
        } else if (ev.key.lkey == LogicalKey::ArrowDown) {
            uint32_t new_sector = state.cur_sector;
            if (new_sector-- != 0) {
                // Mark the new spot
                seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
                seg.data[new_sector * num_slots + state.cur_spot] ^= 2;
                state.cur_sector = new_sector;
                // Regenerate scene
                GenerateLevelSceneModel(seg);
            } // TODO: Move between segments
        } else if (ev.key.lkey == LogicalKey::ArrowUp) {
            uint32_t new_sector = state.cur_sector;
            if (++new_sector >= seg.sectors) {
                // TODO: Move between segments if present
                seg.data.resize(seg.data.size() + num_slots);
                ++seg.sectors;
            }
            // Mark the new spot
            seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 2;
            seg.data[new_sector * num_slots + state.cur_spot] ^= 2;
            state.cur_sector = new_sector;
            // Regenerate scene
            GenerateLevelSceneModel(seg);
        } else if (ev.key.lkey == LogicalKey::Space) {
            // Set/reset the spot
            seg.data[state.cur_sector * num_slots + state.cur_spot] ^= 1;
            // Regenerate scene
            GenerateLevelSceneModel(seg);
        } else if (ev.key.lkey == LogicalKey::P) {
            DumpLevelToFile(state.common->level, "level.dat");
            std::puts("Dumped level to file 'level.dat'");
        } else if (ev.key.lkey == LogicalKey::L) {
            if (LoadLevelFromFile(level, "level.dat")) {
                state.cur_segment = 0;
                state.cur_sector = 0;
                state.cur_spot = 0;
                level.segments[0].data[0] ^= 2;
                GenerateLevelSceneModel(level.segments[0]);
                std::puts("Loaded level 'level.dat'");
            }
        }
    }
}

static void editor_render(void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(state.common->shader.prog);
    glUniform3f(state.common->shader.loc_uScale, 1.f, 1.f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0.f, 0.f, state.curZ);
    RenderLevel(state.common->level);
}

static void editor_switch(void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    GeometrySegment& seg = state.common->level.segments[state.cur_segment];
    seg.data[state.cur_sector * seg.floors * seg.floor_planes + state.cur_spot] ^= 2;
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
    glUniform3f(state.common->shader.loc_uScale, 1.f, 1.f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0.f, 0.f, state.curZ + zfactor);
    RenderLevel(state.common->level);

    glBindVertexArray(state.player_vao);
    glUniform3f(state.common->shader.loc_uScale, state.common->level.segments[0].pwidth, .4f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0., state.common->level.segments[0].yval, zfactor);
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
    SetupSegmentBuffers(state.level.segments[0]);

    // Load level geometry
    GenerateLevelSceneModel(state.level.segments[0]);

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
