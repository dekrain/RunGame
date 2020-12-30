#include <cstdio>
#include <cstdint>
#include <array>
#include <cmath>

#include <SDL2/SDL.h>

#include <GL/glew.h>

#include "util.hpp"
#include "run.hpp"

/*static Vtx sPolygonData[] = {
    {{ .0f,  .5f, -.1f}},
    {{ .8f, -.5f,  0.f}},
    {{-.5f, -.5f,  0.f}}
};*/

// 5 planes per 4 floors per sector
static uint8_t sLevelData[] = {
    // 2 full sectors
    1, 0, 0, 1, 1,   1, 1, 1, 1, 1,   1, 1, 1, 1, 1,   1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,   1, 1, 1, 1, 1,   1, 1, 1, 1, 1,   1, 1, 1, 1, 1,
    // 2 sectors with holes
    1, 0, 1, 0, 1,   1, 1, 1, 1, 1,   0, 0, 0, 0, 0,   1, 1, 1, 1, 1,
    1, 1, 1, 0, 1,   1, 0, 0, 1, 1,   0, 0, 1, 0, 0,   1, 1, 1, 1, 1,
};

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
    uint32_t level_vao, level_vbo;
    LevelInfo level;
    size_t level_vtx_count;
    float floorY, floorW, floorXl, floorXr;
    BasicShader shader;
};

struct EditorState {
    CommonState* common;
    uint32_t cur_sector, cur_spot;
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
    s_ctx.cur_sector = s_ctx.cur_spot = 0;

    s_ctx.curZ = 0.0f;
}

static void editor_input(SDL_Event& ev, void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    if (ev.type == SDL_KEYDOWN) {
        if (ev.key.keysym.sym == SDLK_w) {
            state.curZ += 0.02f;
        } else if (ev.key.keysym.sym == SDLK_s) {
            state.curZ -= 0.02f;
        }
        // select
        else if (ev.key.keysym.sym == SDLK_LEFT) {
            uint32_t new_spot = state.cur_spot;
            if (new_spot-- == 0) {
                new_spot = num_slots - 1;
            }
            // Mark the new spot
            state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 2;
            state.common->level.sector_data[state.cur_sector].data[new_spot] ^= 2;
            state.cur_spot = new_spot;
            // Regenerate scene
            GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
        } else if (ev.key.keysym.sym == SDLK_RIGHT) {
            uint32_t new_spot = state.cur_spot;
            if (++new_spot == num_slots) {
                new_spot = 0;
            }
            // Mark the new spot
            state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 2;
            state.common->level.sector_data[state.cur_sector].data[new_spot] ^= 2;
            state.cur_spot = new_spot;
            // Regenerate scene
            GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
        } else if (ev.key.keysym.sym == SDLK_DOWN) {
            uint32_t new_sector = state.cur_sector;
            if (new_sector-- != 0) {
                // Mark the new spot
                state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 2;
                state.common->level.sector_data[new_sector].data[state.cur_spot] ^= 2;
                state.cur_sector = new_sector;
                // Regenerate scene
                GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
            }
        } else if (ev.key.keysym.sym == SDLK_UP) {
            uint32_t new_sector = state.cur_sector;
            if (++new_sector >= state.common->level.num_sectors) {
                state.common->level.sector_data.emplace_back();
                ++state.common->level.num_sectors;
            }
            // Mark the new spot
            state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 2;
            state.common->level.sector_data[new_sector].data[state.cur_spot] ^= 2;
            state.cur_sector = new_sector;
            // Regenerate scene
            GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
        } else if (ev.key.keysym.sym == SDLK_SPACE) {
            // Set/reset the spot
            state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 1;
            // Regenerate scene
            GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
        } else if (ev.key.keysym.sym == SDLK_p) {
            DumpLevelToFile(state.common->level, "level.dat");
            std::puts("Dumped level to file 'level.dat'");
        } else if (ev.key.keysym.sym == SDLK_l) {
            state.common->level = LoadLevelFromFile("level.dat");
            state.cur_sector = 0;
            state.cur_spot = 0;
            state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 2;
            GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
            std::puts("Loaded level 'level.dat'");
        }
    }
}

static void editor_render(void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(state.common->shader.prog);
    glBindVertexArray(state.common->level_vao);
    glUniform3f(state.common->shader.loc_uScale, 1.f, 1.f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0.f, 0.f, state.curZ);
    glDrawArrays(GL_TRIANGLES, 0, state.common->level_vtx_count);
}

static void editor_switch(void* ctx) {
    EditorState& state = *reinterpret_cast<EditorState*>(ctx);
    state.common->level.sector_data[state.cur_sector].data[state.cur_spot] ^= 2;
    GenerateLevelSceneModel(state.common->level, state.common->level_vbo, state.common->level_vtx_count);
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

static void game_input(SDL_Event& ev, void* ctx) {}

static void game_render(void* ctx) {
    PlayingState& state = *reinterpret_cast<PlayingState*>(ctx);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static float const zfactor = -0.08f;

    state.curZ += state.speed;

    glUseProgram(state.common->shader.prog);
    glBindVertexArray(state.common->level_vao);
    glUniform3f(state.common->shader.loc_uScale, 1.f, 1.f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0.f, 0.f, state.curZ + zfactor);
    glDrawArrays(GL_TRIANGLES, 0, state.common->level_vtx_count);

    glBindVertexArray(state.player_vao);
    glUniform3f(state.common->shader.loc_uScale, state.common->floorW, .4f, sLevelZScale);
    glUniform3f(state.common->shader.loc_uDisplacement, 0., state.common->floorY, zfactor);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void game_switch(void* ctx) {
    PlayingState& state = *reinterpret_cast<PlayingState*>(ctx);
    state.curZ = 0.0f;
    state.speed = 0.002f;
}

static void common_init(CommonState& state) {
    // Init scene
    state.level = LoadLevelFromArray(4, sLevelData);

    glGenVertexArrays(1, &state.level_vao);
    // Load level geometry
    glGenBuffers(1, &state.level_vbo);
    GenerateLevelSceneModel(state.level, state.level_vbo, state.level_vtx_count);

    // Prepare vertex arrays
    glBindVertexArray(state.level_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state.level_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vtx), reinterpret_cast<const void*>(offsetof(Vtx, col)));
    glEnableVertexAttribArray(1);

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

    GetFloorProperties(state.floorY, state.floorW, state.floorXl, state.floorXr);
}

static void common_finish(CommonState& state, EditorState& s_editor, PlayingState& s_playing) {
    glDeleteVertexArrays(1, &state.level_vao);
    glDeleteVertexArrays(1, &s_playing.player_vao);
    glDeleteBuffers(1, &state.level_vbo);
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

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* wnd = SDL_CreateWindow("Test ogl", 0, 0, 800, 800, SDL_WINDOW_OPENGL);
    SDL_GLContext gctx = SDL_GL_CreateContext(wnd);

    SDL_GL_MakeCurrent(wnd, gctx);
    glewExperimental = true;
    glewInit();

    // Main loop
    common_init(s_common);
    editor_init(&s_common, &s_editor);
    game_init(&s_common, &s_game);

    state->change(state_ctx);

    SDL_Event ev;
    while (true) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                goto end_prog;
            else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_b) {
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
        SDL_GL_SwapWindow(wnd);
    }
    end_prog:

    // Deinit
    common_finish(s_common, s_editor, s_game);

    SDL_GL_DeleteContext(gctx);
    SDL_DestroyWindow(wnd);

    SDL_Quit();
}
