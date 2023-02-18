#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

//#include "menu.h"

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 500
#define GRID_SIZE 20
#define BODY_SIZE (GRID_SIZE*GRID_SIZE)

#define MAX_PLAYERS_SIZE 4

#define DIREC_BUFFER_SIZE 2
#define APPLES_SIZE 1000

#define DEAD_BODIES_SIZE 1000

#define SPEEDUP_RATE 0.05
#define MIN_MOVEM_DELAY 80

SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font; 
SDL_Texture* head_text;
SDL_Texture* body_text;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define GRID_DIMENS MIN(WINDOW_WIDTH, WINDOW_HEIGHT)
#define GRID_X0 (WINDOW_WIDTH / 2 - GRID_DIMENS / 2)
#define GRID_Y0 (WINDOW_HEIGHT / 2 - GRID_DIMENS / 2)

struct Pos {
    int x;
    int y;
};

enum Direction {
    DOWN,
    LEFT,
    RIGHT,
    UP
};

void printSdlError(char* message) {
    fputs(">> ", stderr);
    fputs(message, stderr);
    fprintf(stderr, ": %s\n", SDL_GetError());
}

bool init() {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printSdlError("SDL_Init error");
        return false;
    }

    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
        printSdlError("IMG_INIT error");
        return false;
    }

    if (TTF_Init() != 0) {
        printSdlError("TTL_Init");
        return false;
    }
        
    window = SDL_CreateWindow("Titulo",
        0, 0,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0
    );
    if (!window) {
       printSdlError("SDL_CreateWindow"); 
       return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printSdlError("SDL_CreateRenderer");
        return false;
    }

    return true;
}

bool loadMedia() {
    // Load font
    font = TTF_OpenFont("COMIC.TTF", 24);
    if (!font) {
        printSdlError("Loading COMIC.TTF");
        return false;
    }

    SDL_Surface* head_surf = IMG_Load("head.png");
    if (!head_surf) {
        printSdlError("Loading head.png");
        TTF_CloseFont(font);
        return false;
    }
    head_text = SDL_CreateTextureFromSurface(renderer, head_surf);
    SDL_FreeSurface(head_surf);

    SDL_Surface* body_surf = IMG_Load("body.png");
    if (!body_surf) {
        printSdlError("Loading body.png");
        SDL_DestroyTexture(head_text);
        TTF_CloseFont(font);
        return false;
    }
    body_text = SDL_CreateTextureFromSurface(renderer, body_surf);
    SDL_FreeSurface(body_surf);

    return true;
}

#define POWERUP_SIZE 3

enum Powerup {
    NONE,
    ZOMBIE,
    SONIC,
};

struct Apple {
    struct Pos pos;
    enum Powerup type;
};

void appleInit(struct Apple* p_apple) {
    p_apple->pos.x = rand() % GRID_SIZE;
    p_apple->pos.y = rand() % GRID_SIZE;

    int odds[POWERUP_SIZE];
    odds[NONE] = 10; 
    odds[ZOMBIE] = 1;
    odds[SONIC] = 1;

    int total = 0;
    for (size_t i = 0; i < POWERUP_SIZE; i++) {
        total += odds[i];
    }

    int r = rand() % total;

    int accum = 0;
    for (size_t i = 0; i < POWERUP_SIZE; i++) {
        if (r < odds[i] + accum) {
            p_apple->type = (enum Powerup)i;
            break;
        }
        accum += odds[i];
    }
}

struct AppleSpawner {
    struct Apple apples[APPLES_SIZE];
    size_t apples_size;

    uint32_t last_spawn_frame;
    uint32_t spawn_delay;
};

void appleSpawnerInit(struct AppleSpawner* p_apple_spawner) {
    for (size_t i = 0; i < p_apple_spawner->apples_size; i++) {
        appleInit(&p_apple_spawner->apples[i]);
    }
    p_apple_spawner->apples_size = 1;
    p_apple_spawner->last_spawn_frame = 0;
    p_apple_spawner->spawn_delay = 3000;
}

struct Player {
    int score;
    bool game_over;

    uint32_t last_movem_frame;
    uint32_t movem_delay;

    struct Pos pos;
    struct Pos body[BODY_SIZE];
    size_t body_size; 

    enum Direction direc;
    enum Direction direc_buff[DIREC_BUFFER_SIZE];
    int direc_size;
    int direc_i;
    bool reset_buffer_on_input;

    uint32_t zombie_end;
    uint32_t zombie_duration;

    uint32_t sonic_end;
    uint32_t sonic_duration;

    SDL_Keycode bindings[4];
};

void playerInit(struct Player* p_player) {
    p_player->score = 0;
    p_player->game_over = false;

    p_player->last_movem_frame = 0;
    p_player->movem_delay = 250;

    p_player->pos.x = 0;
    p_player->pos.y = 0;

    p_player->body_size = 0;
    
    p_player->direc = RIGHT;
    p_player->direc_size = 0;
    p_player->direc_i = 0;
    p_player->reset_buffer_on_input = true;

    p_player->zombie_end = 0;
    p_player->zombie_duration = 3000;

    p_player->sonic_end = 0;
    p_player->sonic_duration = 3000;
}

struct GameState {
    struct AppleSpawner apple_spawner;
    struct Player players[MAX_PLAYERS_SIZE];
    size_t players_size;

    struct Pos dead_bodies[DEAD_BODIES_SIZE];
    size_t dead_bodies_size;
};


void playerOnInput(struct Player* p_player, SDL_Keycode key) {
    enum Direction direc;
    bool moved = false;

    for (size_t i = 0; i < 4; i++) {
        if (p_player->bindings[i] == key) {
            direc = (enum Direction)i;
            moved = true;
        }
    } 

    if (moved) {
        if (p_player->reset_buffer_on_input) {
            p_player->reset_buffer_on_input = false;
            p_player->direc_size = 0;
            p_player->direc_i = 0;
        }
        if (p_player->direc_size < DIREC_BUFFER_SIZE) {
            p_player->direc_buff[p_player->direc_size++] = direc;
        }
    }
}

/*
 * All players must move their heads and bodies before checking collision
 * */
void gameStateUpdate(struct GameState* game_state, uint32_t curr_time) {
    // Initialize to false
    bool move[MAX_PLAYERS_SIZE] = {0};

    // Check if player will move
    for (size_t i = 0; i < game_state->players_size; i++) {
        if (game_state->players[i].game_over) {
            continue;
        }

        uint32_t movem_delay = game_state->players[i].movem_delay;
        if (curr_time < game_state->players[i].sonic_end) {
            movem_delay /= 2;
        }

        if (curr_time - game_state->players[i].last_movem_frame > movem_delay) {
            move[i] = true;  
        }
    }

    // Move heads
    struct Pos last_pos[MAX_PLAYERS_SIZE];
    for (size_t i = 0; i < game_state->players_size; i++) {
        if (game_state->players[i].game_over || !move[i]) continue;

        game_state->players[i].last_movem_frame = curr_time;
        game_state->players[i].reset_buffer_on_input = true;

        // Store last position
        last_pos[i] = game_state->players[i].pos;

        // Check input in buffer
        if (game_state->players[i].direc_i < game_state->players[i].direc_size) {
            game_state->players[i].direc = game_state->players[i].direc_buff[game_state->players[i].direc_i++];
        }

        // Move
        switch (game_state->players[i].direc) {
        case DOWN:
            game_state->players[i].pos.y++;
        break;
        case LEFT:
            game_state->players[i].pos.x--;
        break;
        case RIGHT:
            game_state->players[i].pos.x++;
        break;
        case UP:
            game_state->players[i].pos.y--;
        break;
        }

        // Wraparound
        if (game_state->players[i].pos.x < 0) {
            game_state->players[i].pos.x = GRID_SIZE-1;
        } else if (game_state->players[i].pos.x >= GRID_SIZE) {
            game_state->players[i].pos.x = 0;
        }

        if (game_state->players[i].pos.y < 0) {
            game_state->players[i].pos.y = GRID_SIZE-1;
        } else if (game_state->players[i].pos.y >= GRID_SIZE) {
            game_state->players[i].pos.y = 0;
        }
    }

    // Check apples
    for (size_t p_i = 0; p_i < game_state->players_size; p_i++) {
        if (game_state->players[p_i].game_over) continue;

        for (size_t a_i = 0; a_i < game_state->apple_spawner.apples_size; a_i++) {
            if (game_state->players[p_i].pos.x == game_state->apple_spawner.apples[a_i].pos.x
                    && game_state->players[p_i].pos.y == game_state->apple_spawner.apples[a_i].pos.y) {
                game_state->players[p_i].score++;

                switch (game_state->apple_spawner.apples[a_i].type) {
                case NONE: {
                } break;
                case ZOMBIE: {
                    game_state->players[p_i].zombie_end = curr_time + game_state->players[p_i].zombie_duration;
                } break;
                case SONIC: {
                    game_state->players[p_i].sonic_end = curr_time + game_state->players[p_i].sonic_duration;
                } break;
                }
                
                appleInit(&game_state->apple_spawner.apples[a_i]);

                game_state->players[p_i].body_size++; 
            }
        }
    }

    // Move body and zombie
    for (size_t p_i = 0; p_i < game_state->players_size; p_i++) {
        if (game_state->players[p_i].game_over || !move[p_i]) continue;
    
        // Move body
        if (game_state->players[p_i].body_size > 0) {
            for (size_t b_i = game_state->players[p_i].body_size-1; b_i > 0; b_i--) {
                game_state->players[p_i].body[b_i] = game_state->players[p_i].body[b_i-1]; 
            }
            game_state->players[p_i].body[0] = last_pos[p_i];
        }

        // Add zombie dead body
        if (curr_time < game_state->players[p_i].zombie_end) {
            if (game_state->players[p_i].body_size > 0) {
                game_state->dead_bodies[game_state->dead_bodies_size] = game_state->players[p_i].body[game_state->players[p_i].body_size-1];

                game_state->dead_bodies_size++;
                game_state->players[p_i].body_size--;
            }
        }
    }

    // Check colision
    for (size_t this_i = 0; this_i < game_state->players_size; this_i++) {
        if (game_state->players[this_i].game_over) continue;

        for (size_t other_i = 0; other_i < game_state->players_size; other_i++) {
            if (this_i != other_i
                    && game_state->players[this_i].pos.x == game_state->players[other_i].pos.x 
                    && game_state->players[this_i].pos.y == game_state->players[other_i].pos.y) {
                game_state->players[this_i].game_over = true;
            }

            for (size_t k = 0; k < game_state->players[other_i].body_size; k++) {
                struct Pos* p_body = &game_state->players[other_i].body[k];
                
                if (game_state->players[this_i].pos.x == p_body->x && game_state->players[this_i].pos.y == p_body->y) {
                    game_state->players[this_i].game_over = true;
                }    
            }
        }

        for (size_t d_i = 0; d_i < game_state->dead_bodies_size; d_i++) {
            if (game_state->players[this_i].pos.x == game_state->dead_bodies[d_i].x
                && game_state->players[this_i].pos.y == game_state->dead_bodies[d_i].y) {
                game_state->players[this_i].game_over = true;
            }
        }
    }

    // Spawn apples
    if (curr_time - game_state->apple_spawner.last_spawn_frame > game_state->apple_spawner.spawn_delay) {
        game_state->apple_spawner.last_spawn_frame = curr_time;
        appleInit(&game_state->apple_spawner.apples[game_state->apple_spawner.apples_size++]);
    } 
}

void renderMsg(char* msg, SDL_Rect* hitbox, SDL_Color color) {
    SDL_Color font_color = color;

    SDL_Surface* surf = TTF_RenderText_Solid(font, msg, font_color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    SDL_RenderCopy(renderer, texture, NULL, hitbox);

    SDL_DestroyTexture(texture);
}

void renderMsgsCentered(char** texts, size_t button_qty, SDL_Rect* hitbox, SDL_Color* colors) {
    int total_height = 0;

    for (size_t i = 0; i < button_qty; i++) {
        int h;
        TTF_SizeText(font, texts[i], NULL, &h);
        total_height += h;
    }
    
    int y = WINDOW_HEIGHT/2 - total_height/2; 

    if (y < 0) assert(false && "Too large menu buttons");

    for (size_t i = 0; i < button_qty; i++) {
        SDL_Rect button_rect;
        TTF_SizeText(font, texts[i], &button_rect.w, &button_rect.h);
        button_rect.x = WINDOW_WIDTH/2 - button_rect.w/2;
        button_rect.y = y;

        renderMsg(texts[i], &button_rect, colors[i]);

        y += button_rect.h; 
        hitbox[i] = button_rect;
    }
}

SDL_Rect posToRect(struct Pos* p_pos) {
    SDL_Rect rect;
    rect.x = GRID_X0 + p_pos->x * GRID_DIMENS / GRID_SIZE;
    rect.y = GRID_Y0 + p_pos->y * GRID_DIMENS / GRID_SIZE;
    rect.w = GRID_DIMENS / GRID_SIZE;
    rect.h = GRID_DIMENS / GRID_SIZE;

    return rect;
}

void playerRenderBody(struct Player* p_player) {
    for (size_t i = 0; i < p_player->body_size; i++) {
        SDL_Rect body_rect = posToRect(&p_player->body[i]);
        SDL_RenderCopy(renderer, body_text, NULL, &body_rect);
    }
}

void playerRenderHead(struct Player* p_player) {
    SDL_Rect head_rect = posToRect(&p_player->pos);

    double rotation;
    switch (p_player->direc) {
    case DOWN:
        rotation = 180;
    break;
    case RIGHT:
        rotation = 90;
    break;
    case LEFT:
        rotation = 270;
    break;
    case UP:
        rotation = 0;
    break;
    }
    
    SDL_RenderCopyEx(renderer, head_text, NULL, &head_rect, rotation, NULL, 0);
}

void renderPlayersScore(struct Player* players, size_t players_size) {
    int y = 0;

    char message[] = "Player 1: 000";
    size_t len = strlen(message);
    
    for (size_t i = 0; i < players_size; i++) {
        message[7] = (i + 1) + '0';

        sprintf(message+len-3, "%3d", players[i].score);

        SDL_Color score_color = {0x56, 0x73, 0x45, 255};

        SDL_Rect score_rect;
        TTF_SizeText(font, message, &score_rect.w, &score_rect.h);
        score_rect.x = 0;
        score_rect.y = y;

        renderMsg(message, &score_rect, score_color);

        y += score_rect.h;
    }
}

enum Mode {
    CHOOSE_NETWORK,
    MENU,
    REMAP_MENU,
    OPTIONS_MENU,
    RUNNING,
    GAME_OVER
};

void reset(struct GameState* game_state) {
    appleSpawnerInit(&game_state->apple_spawner);
    for (size_t i = 0; i < MAX_PLAYERS_SIZE; i++) {
        playerInit(&game_state->players[i]);
    }

    struct Pos init_pos[MAX_PLAYERS_SIZE] = {
        {0, 0},
        {GRID_SIZE-1, 0},
        {0, GRID_SIZE-1},
        {GRID_SIZE-1, GRID_SIZE-1} 
    };

    enum Direction init_direc[MAX_PLAYERS_SIZE] = {
        RIGHT,
        LEFT,
        RIGHT,
        LEFT  
    };

    for (size_t i = 0; i < MAX_PLAYERS_SIZE; i++) {
        game_state->players[i].pos = init_pos[i];
        game_state->players[i].direc = init_direc[i];
    }

    game_state->dead_bodies_size = 0;
}


bool rectContainsPos(SDL_Rect* rect, struct Pos* pos) {
    return pos->x > rect->x
        && pos->x < rect->x + rect->w
        && pos->y > rect->y
        && pos->y < rect->y + rect->h;
}

void render(struct GameState* game_state) {
    // Background
    SDL_SetRenderDrawColor(renderer, 0x3C, 0xDF, 0xFF, 255);
    SDL_RenderClear(renderer);

    // Grid
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 255);
    SDL_Rect grid_rect = {.x = GRID_X0, .y = GRID_Y0, .w = GRID_DIMENS, .h = GRID_DIMENS};
    SDL_RenderFillRect(renderer, &grid_rect);

    // Apple
    SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 255);
    for (size_t i = 0; i < game_state->apple_spawner.apples_size; i++) {
        switch (game_state->apple_spawner.apples[i].type) {
        case NONE: {
            SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 255);
        } break;
        case ZOMBIE: {
            SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0, 255);
        } break;
        case SONIC: {
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 255);
        } break;
        }

        SDL_Rect apple_rect = posToRect(&game_state->apple_spawner.apples[i].pos);
        SDL_RenderFillRect(renderer, &apple_rect);
    }

    // Dead bodies
    for (size_t i = 0; i < game_state->dead_bodies_size; i++) {
        SDL_SetRenderDrawColor(renderer, 0x02, 0x30, 0x20, 0xFF);
        SDL_Rect rect = posToRect(&game_state->dead_bodies[i]);
        SDL_RenderFillRect(renderer, &rect);
    }

    // Body
    for (size_t i = 0; i < game_state->players_size; i++) {
        playerRenderBody(&game_state->players[i]);
    }

    // Snake Head
    for (size_t i = 0; i < game_state->players_size; i++) {
        playerRenderHead(&game_state->players[i]);
    }

    // Score
    renderPlayersScore(game_state->players, game_state->players_size);
}

int main() {
    srand(time(NULL));

    if (!init()) {
        goto cleanup; 
    }

    if (!loadMedia()) {
        goto cleanup;
    }

    // Init game structs
    struct GameState game_state = {
        .players_size = 1,
        .dead_bodies_size = 0,
    };

    reset(&game_state);

    // Set initial bindings
    SDL_Keycode bindings[MAX_PLAYERS_SIZE][4] = {
        {SDLK_s, SDLK_a, SDLK_d, SDLK_w},
        {SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_UP},
        {SDLK_g, SDLK_f, SDLK_h, SDLK_t},
        {SDLK_k, SDLK_j, SDLK_l, SDLK_i}
    };

    for (size_t i = 0; i < MAX_PLAYERS_SIZE; i++) {
        memcpy(&game_state.players[i].bindings, bindings[i], sizeof(SDL_Keycode)*4);
    }

    enum Mode mode = CHOOSE_NETWORK;
    bool already_running = false;

    enum NetworkOptions {
        NO_LOCAL,
        NO_ONLINE,
    };

    struct ChooseNetwork {
        enum NetworkOptions option; 
    } choose_network;

    struct GameOver {
        uint32_t delay;
        uint32_t start;
    } game_over = {
        .delay = 1000,
    };

    struct Menu {
        SDL_Color button_color;
    } menu = {
        .button_color = {0xFF, 0x00, 0x00, 0xFF},
    };

    struct RemapMenu {
        SDL_Color button_sel_color;
        bool button_sel;
        size_t button_sel_i;
        size_t sel_player_i;
    } remap_menu = {
        .button_sel_color = {0x00, 0xFF, 0x00, 0xFF},
        .button_sel = false,
        .sel_player_i = 0,
    };

    // Main switch
    while (true) {
        uint32_t curr_time = SDL_GetTicks();
        switch (mode) {
        case CHOOSE_NETWORK: {
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(renderer);

#define CHOOSE_NETWORK_MSGS_SIZE 3

            char* msg[CHOOSE_NETWORK_MSGS_SIZE] = {
                "Local",
                "Online",
                "Quit",
            };

            enum CHOOSE_NETWORK_OPTIONS {
                CN_LOCAL,
                CN_ONLINE,
                CN_QUIT,
            };

            SDL_Rect hitbox[CHOOSE_NETWORK_MSGS_SIZE];

            SDL_Color color[CHOOSE_NETWORK_MSGS_SIZE];
            for (size_t i = 0; i < CHOOSE_NETWORK_MSGS_SIZE; i++) {
                color[i] = menu.button_color;
            }

            renderMsgsCentered(msg, CHOOSE_NETWORK_MSGS_SIZE, hitbox, color);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: {
                    goto cleanup_media;
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    struct Pos mouse_pos = {.x = event.button.x, .y = event.button.y};

                    for (size_t i = 0; i < CHOOSE_NETWORK_MSGS_SIZE; i++) {
                        if (rectContainsPos(&hitbox[i], &mouse_pos)) {
                            switch ((enum CHOOSE_NETWORK_OPTIONS)i) {
                            case CN_LOCAL: {
                                mode = MENU;
                            } break; 
                            case CN_ONLINE: {
                                // @todo
                            } break;
                            case CN_QUIT: {
                                goto cleanup_media;
                            } break;
                            }  
                        } 
                    }
                } break;
                }
            }

        } break;
        case MENU: {

#define MENU_BUTTONS_SIZE 3

            // clean screen
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(renderer);

            // draw buttons
            enum Button {
                M_START,
                M_OPTIONS,
                M_BACK
            };

            char* msg[MENU_BUTTONS_SIZE] = {"Start", "Options", "Back"};

            if (already_running) {
                msg[0] = "Continue";
            }

            SDL_Color colors[MENU_BUTTONS_SIZE]; 
            for (size_t i = 0; i < MENU_BUTTONS_SIZE; i++) {
                colors[i] = menu.button_color;                
            }

            SDL_Rect hitbox[MENU_BUTTONS_SIZE];
            renderMsgsCentered(msg, MENU_BUTTONS_SIZE, hitbox, colors);

            // events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: {
                    goto cleanup_media; 
                } break;
                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: {
                        if (already_running) {
                            mode = RUNNING;
                        } else {
                            mode = CHOOSE_NETWORK;
                        }
                    } break;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    for (size_t i = 0; i < MENU_BUTTONS_SIZE; i++) {
                        struct Pos mouse_pos = {.x = event.button.x, .y = event.button.y};
                        if (rectContainsPos(&hitbox[i], &mouse_pos)) {
                            switch ((enum Button)i) {
                            case M_START: {
                                mode = RUNNING;
                                already_running = true;
                            } break;
                            case M_OPTIONS: {
                                mode = OPTIONS_MENU;
                            } break;
                            case M_BACK: {
                                mode = CHOOSE_NETWORK;
                                reset(&game_state);
                                already_running = false;
                            } break;
                            }
                        }
                    }
                } break;
                } 
            }
        } break;
        case REMAP_MENU: {
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(renderer);

            // Select player button
            SDL_Rect sel_player_hitbox;
            {
                char msg[9] = "Player 0";

                msg[7] = remap_menu.sel_player_i + 1 + '0';

                sel_player_hitbox.x = 0;
                sel_player_hitbox.y = 0;
                TTF_SizeText(font, msg, &sel_player_hitbox.w, &sel_player_hitbox.h);

                renderMsg(msg, &sel_player_hitbox, menu.button_color);
            }

            // Centered buttons
#define REMAP_MENU_BUTTONS_SIZE 4

            SDL_Rect hitbox[REMAP_MENU_BUTTONS_SIZE];
            {
                char msg[REMAP_MENU_BUTTONS_SIZE][9];
                // It has to be in this order
                char* pre_msg[REMAP_MENU_BUTTONS_SIZE] = {
                    "DOWN: 0",
                    "LEFT: 0",
                    "RIGHT: 0",
                    "UP: 0"
                };

                for (size_t i = 0; i < REMAP_MENU_BUTTONS_SIZE; i++) {
                    strcpy(msg[i], pre_msg[i]);
                }

                for (size_t i = 0; i < REMAP_MENU_BUTTONS_SIZE; i++) {
                    size_t len = strlen(msg[i]);

                    SDL_Keycode key = game_state.players[remap_menu.sel_player_i].bindings[i];
                    
                    if (key <= 0x7F) {
                        msg[i][len-1] = (char)key;
                    } else {
                        switch (key) {
                        case SDLK_DOWN: {
                            msg[i][len-1] = 'v';
                        } break;
                        case SDLK_LEFT: {
                            msg[i][len-1] = '<';
                        } break;
                        case SDLK_RIGHT: {
                            msg[i][len-1] = '>';
                        } break;
                        case SDLK_UP: {
                            msg[i][len-1] = '^';
                        } break;
                        }
                    }
                }

                char* p_msg[REMAP_MENU_BUTTONS_SIZE];
                for (size_t i = 0; i < REMAP_MENU_BUTTONS_SIZE; i++) {
                    p_msg[i] = msg[i]; 
                }

                SDL_Color colors[REMAP_MENU_BUTTONS_SIZE] = {
                    menu.button_color,
                    menu.button_color,
                    menu.button_color,
                    menu.button_color,
                };

                if (remap_menu.button_sel) colors[remap_menu.button_sel_i] = remap_menu.button_sel_color;

                renderMsgsCentered(p_msg, REMAP_MENU_BUTTONS_SIZE, hitbox, colors);
            }

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: {
                    goto cleanup_media; 
                } break;
                case SDL_KEYDOWN: {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mode = OPTIONS_MENU;
                    }
                    if (remap_menu.button_sel) {
                        remap_menu.button_sel = false;
                        game_state.players[remap_menu.sel_player_i].bindings[remap_menu.button_sel_i] = event.key.keysym.sym;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    struct Pos mouse_pos = {.x = event.button.x, .y = event.button.y};
                    bool pressed_button = false;
                    for (size_t i = 0; i < REMAP_MENU_BUTTONS_SIZE; i++) {
                        if (!remap_menu.button_sel && rectContainsPos(&hitbox[i], &mouse_pos)) {
                            pressed_button = true;
                            remap_menu.button_sel = true;
                            remap_menu.button_sel_i = i;
                        } 
                    }
                    if (!pressed_button) {
                        remap_menu.button_sel = false;
                    }

                    if (rectContainsPos(&sel_player_hitbox, &mouse_pos)) {
                        remap_menu.sel_player_i++;
                        if (remap_menu.sel_player_i >= game_state.players_size) {
                            remap_menu.sel_player_i = 0;
                        }
                    }
                } break;
                }
            }
        } break;
        case OPTIONS_MENU: {
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(renderer);

#define OPTIONS_MENU_BUTTONS_SIZE 2

            SDL_Rect hitbox[OPTIONS_MENU_BUTTONS_SIZE];
            char msg[OPTIONS_MENU_BUTTONS_SIZE][11];

            enum Options {
                PLAYERS,
                REMAP
            };

            // It has to be in this order
            char* pre_msg[OPTIONS_MENU_BUTTONS_SIZE] = {
                "Players: 0",
                "Remap"
            };

            for (size_t i = 0; i < OPTIONS_MENU_BUTTONS_SIZE; i++) {
                strcpy(msg[i], pre_msg[i]);
            }

            size_t len = strlen(msg[PLAYERS]);
            msg[PLAYERS][len-1] = game_state.players_size + '0';

            char* p_msg[OPTIONS_MENU_BUTTONS_SIZE];
            for (size_t i = 0; i < OPTIONS_MENU_BUTTONS_SIZE; i++) {
                p_msg[i] = msg[i]; 
            }

            SDL_Color colors[OPTIONS_MENU_BUTTONS_SIZE] = {
                menu.button_color,
                menu.button_color,
            };

            renderMsgsCentered(p_msg, OPTIONS_MENU_BUTTONS_SIZE, hitbox, colors);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: {
                    goto cleanup_media; 
                } break;
                case SDL_KEYDOWN: {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mode = MENU;
                    }
                    if (remap_menu.button_sel) {
                        remap_menu.button_sel = false;
                        game_state.players[remap_menu.sel_player_i].bindings[remap_menu.button_sel_i] = event.key.keysym.sym;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    struct Pos mouse_pos = {.x = event.button.x, .y = event.button.y};
                    if (rectContainsPos(&hitbox[PLAYERS], &mouse_pos)) {
                        game_state.players_size++;
                        if (game_state.players_size > MAX_PLAYERS_SIZE) {
                            game_state.players_size = 1;
                        }
                    }
                    if (rectContainsPos(&hitbox[REMAP], &mouse_pos)) {
                        mode = REMAP_MENU;
                    }
                } break;
                }
            }
        } break;
        case RUNNING: {
            bool all_died = true;
            for (size_t i = 0; i < game_state.players_size; i++) {
                if (!game_state.players[i].game_over) all_died = false;
            }

            if (all_died) {
                mode = GAME_OVER;
                game_over.start = curr_time;
                already_running = false;
            }

            // ==========
            // Input
            // ==========
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT:
                    goto cleanup_media;
                break;
                case SDL_KEYDOWN:
                    for (size_t i = 0; i < game_state.players_size; i++) {
                        if (!game_state.players[i].game_over) {
                            playerOnInput(&game_state.players[i], event.key.keysym.sym);
                        }
                    }
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mode = MENU;
                    }
                break;
                }
            }

            // Update
            gameStateUpdate(&game_state, curr_time);

            // Render
            for (size_t i = 0; i < game_state.players_size; i++) {
                if (game_state.players[i].score > 999) {
                    fprintf(stderr, "Score too big!\n");
                    goto cleanup_media;
                }
            }
            render(&game_state);
        } break;
        case GAME_OVER: {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: {
                    goto cleanup_media; 
                } break;
                }
            }

            if (curr_time - game_over.start > game_over.delay) {
                mode = MENU;
                reset(&game_state);
            }

            SDL_Color font_color = {0xFF, 0x00, 0, 255};

            int max_score = 0;
            size_t winner;
            bool draw = false;
            for (size_t i = 0; i < game_state.players_size; i++) {
                if (i == 0) max_score = game_state.players[i].score;
                else if (game_state.players[i].score == max_score) {
                    draw = true;
                } else if (game_state.players[i].score > max_score) {
                    max_score = game_state.players[i].score;
                    winner = i;
                    draw = false;
                }     
            }

            char msg[14];

            if (draw) strcpy(msg, "Draw");
            else {
                strcpy(msg, "Player 0 wins"); 
                msg[7] = winner + 1 + '0';
            }

            SDL_Surface* game_over_surf = TTF_RenderText_Solid(font, msg, font_color);
            SDL_Texture* game_over_text = SDL_CreateTextureFromSurface(renderer, game_over_surf);
            SDL_FreeSurface(game_over_surf);

            SDL_Rect game_over_rect;
            TTF_SizeText(font, msg, &game_over_rect.w, &game_over_rect.h);
            game_over_rect.x = WINDOW_WIDTH/2 - game_over_rect.w/2;
            game_over_rect.y = WINDOW_HEIGHT/2 - game_over_rect.h/2;
            SDL_RenderCopy(renderer, game_over_text, NULL, &game_over_rect);

            SDL_DestroyTexture(game_over_text);
        }; break;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(1000 / 60);
    }

    // =============
    // Cleanup
    // =============
cleanup_media:
    SDL_DestroyTexture(body_text);
    SDL_DestroyTexture(head_text);
    TTF_CloseFont(font);

cleanup:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
