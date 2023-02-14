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
#define APPLES_SIZE 100

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

struct Apple {
    struct Pos pos;
};

void appleInit(struct Apple* p_apple) {
    p_apple->pos.x = rand() % GRID_SIZE;
    p_apple->pos.y = rand() % GRID_SIZE;
}

struct Game {
    struct Apple apples[APPLES_SIZE];
    size_t apples_size;
    uint32_t last_spawn_frame;
    uint32_t spawn_delay;
};

void gameInit(struct Game* p_game) {
    appleInit(&p_game->apples[0]);
    p_game->apples_size = 1;
    p_game->last_spawn_frame = 0;
    p_game->spawn_delay = 10000;
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
}

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

void playersUpdate(
    struct Player* players, size_t players_size,
    uint32_t curr_time,
    struct Apple* apples, size_t apples_size
) {
    // move
    // @todo move out macro
    struct Pos last_tail_pos[MAX_PLAYERS_SIZE];
    for (size_t i = 0; i < players_size; i++) {
        if (players[i].game_over) continue;

        struct Player* p_player = &players[i];

        if (curr_time - p_player->last_movem_frame > p_player->movem_delay) {
            p_player->last_movem_frame = curr_time;
            p_player->reset_buffer_on_input = true;

            struct Pos last_pos = p_player->pos;
            if (p_player->body_size > 0) {
                last_tail_pos[i] = p_player->body[p_player->body_size-1];
            } else {
                last_tail_pos[i] = last_pos;
            }

            // move head
            if (p_player->direc_i < p_player->direc_size) {
                p_player->direc = p_player->direc_buff[p_player->direc_i++];
            }

            switch (p_player->direc) {
            case DOWN:
                p_player->pos.y++;
            break;
            case LEFT:
                p_player->pos.x--;
            break;
            case RIGHT:
                p_player->pos.x++;
            break;
            case UP:
                p_player->pos.y--;
            break;
            }

            if (p_player->pos.x < 0) {
                p_player->pos.x = GRID_SIZE-1;
            } else if (p_player->pos.x >= GRID_SIZE) {
                p_player->pos.x = 0;
            }

            if (p_player->pos.y < 0) {
                p_player->pos.y = GRID_SIZE-1;
            } else if (p_player->pos.y >= GRID_SIZE) {
                p_player->pos.y = 0;
            }

            // move body
            if (p_player->body_size > 0) {
                for (size_t i = p_player->body_size-1; i > 0; i--) {
                    p_player->body[i] = p_player->body[i-1]; 
                }
                p_player->body[0] = last_pos;
            }
        }
    }

    // check apples
    for (size_t i = 0; i < players_size; i++) {
        struct Player* p_player = &players[i];

        if (p_player->game_over) continue;

        for (size_t j = 0; j < apples_size; j++) {
            if (p_player->pos.x == apples[j].pos.x
                    && p_player->pos.y == apples[j].pos.y) {
                p_player->score++;
                
                appleInit(&apples[j]);

                p_player->body_size++; 
                p_player->body[p_player->body_size-1] = last_tail_pos[i];
            }
        }
    }

    // check colision
    for (size_t i = 0; i < players_size; i++) {
        struct Player* p_this = &players[i];

        if (p_this->game_over) continue;

        for (size_t j = 0; j < players_size; j++) {
            struct Player* p_other = &players[j];

            if (p_this != p_other 
                    && p_this->pos.x == p_other->pos.x 
                    && p_this->pos.y == p_other->pos.y) {
                p_this->game_over = true;
            }

            for (size_t k = 0; k < p_other->body_size; k++) {
                struct Pos* p_body = &p_other->body[k];
                
                if (p_this->pos.x == p_body->x && p_this->pos.y == p_body->y) {
                    p_this->game_over = true;
                }    
            }
        }
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
    MENU,
    OPTIONS_MENU,
    RUNNING,
    GAME_OVER
};

void reset(struct Game* p_game, struct Player* players, size_t players_size) {
    gameInit(p_game);
    for (size_t i = 0; i < players_size; i++) {
        playerInit(&players[i]);
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

    for (size_t i = 0; i < players_size; i++) {
        players[i].pos = init_pos[i];
        players[i].direc = init_direc[i];
    }
}


bool rectContainsPos(SDL_Rect* rect, struct Pos* pos) {
    return pos->x > rect->x
        && pos->x < rect->x + rect->w
        && pos->y > rect->y
        && pos->y < rect->y + rect->h;
}

int main() {
    srand(time(NULL));

    if (!init()) {
        goto cleanup; 
    }

    if (!loadMedia()) {
        goto cleanup;
    }

    enum Mode mode = MENU;

    struct Game game;
    struct Player players[MAX_PLAYERS_SIZE];
    size_t players_size = 2;

    reset(&game, players, MAX_PLAYERS_SIZE);

    SDL_Keycode bindings[MAX_PLAYERS_SIZE][4] = {
        {SDLK_s, SDLK_a, SDLK_d, SDLK_w},
        {SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_UP},
        {SDLK_g, SDLK_f, SDLK_h, SDLK_t},
        {SDLK_k, SDLK_j, SDLK_l, SDLK_i}
    };

    for (size_t i = 0; i < MAX_PLAYERS_SIZE; i++) {
        memcpy(&players[i].bindings, bindings[i], sizeof(SDL_Keycode)*4);
    }

    uint32_t game_over_delay = 1000;
    uint32_t game_over_start;

    bool already_running = false;

    SDL_Color button_released_color = {0xFF, 0x00, 0x00, 0xFF};
    SDL_Color button_pressed_color = {0x00, 0xFF, 0x00, 0xFF};

    bool remap_button_sel = false;
    size_t remap_button_sel_i; 

    size_t sel_player_i = 0;

    while (true) {
        uint32_t curr_time = SDL_GetTicks();
        switch (mode) {
        case MENU: {

#define MENU_BUTTONS_SIZE 3

            // clean screen
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(renderer);

            // draw buttons
            enum Button {
                START,
                OPTIONS,
                QUIT
            };

            char* msg[MENU_BUTTONS_SIZE] = {"Start", "Options", "Quit"};

            if (already_running) {
                msg[0] = "Continue";
            }

            SDL_Color colors[MENU_BUTTONS_SIZE] = {
                button_released_color,
                button_released_color,   
                button_released_color   
            };

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
                        mode = RUNNING;
                    } break;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    for (size_t i = 0; i < MENU_BUTTONS_SIZE; i++) {
                        struct Pos mouse_pos = {.x = event.button.x, .y = event.button.y};
                        if (rectContainsPos(&hitbox[i], &mouse_pos)) {
                            switch ((enum Button)i) {
                            case START: {
                                mode = RUNNING;
                                already_running = true;
                            } break;
                            case OPTIONS: {
                                mode = OPTIONS_MENU;
                            } break;
                            case QUIT: {
                                goto cleanup_media;
                            } break;
                            }
                        }
                    }
                } break;
                } 
            }
        } break;
        case OPTIONS_MENU: {
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(renderer);

            // =====================
            // Change player button
            // =====================
            SDL_Rect sel_player_hitbox;
            {
                char msg[9] = "Player 0";

                msg[7] = sel_player_i + 1 + '0';

                sel_player_hitbox.x = 0;
                sel_player_hitbox.y = 0;
                TTF_SizeText(font, msg, &sel_player_hitbox.w, &sel_player_hitbox.h);

                renderMsg(msg, &sel_player_hitbox, button_released_color);
            }

            // ===================
            // Centered buttons
            // ===================
#define OPTIONS_MENU_BUTTONS_SIZE 4

            SDL_Rect hitbox[OPTIONS_MENU_BUTTONS_SIZE];
            {
                char msg[OPTIONS_MENU_BUTTONS_SIZE][9];
                // It has to be in this order
                char* pre_msg[OPTIONS_MENU_BUTTONS_SIZE] = {
                    "DOWN: 0",
                    "LEFT: 0",
                    "RIGHT: 0",
                    "UP: 0"
                };

                for (size_t i = 0; i < OPTIONS_MENU_BUTTONS_SIZE; i++) {
                    strcpy(msg[i], pre_msg[i]);
                }

                for (size_t i = 0; i < OPTIONS_MENU_BUTTONS_SIZE; i++) {
                    size_t len = strlen(msg[i]);

                    SDL_Keycode key = players[sel_player_i].bindings[i];
                    
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

                char* p_msg[OPTIONS_MENU_BUTTONS_SIZE];
                for (size_t i = 0; i < OPTIONS_MENU_BUTTONS_SIZE; i++) {
                    p_msg[i] = msg[i]; 
                }

                SDL_Color colors[OPTIONS_MENU_BUTTONS_SIZE] = {
                    button_released_color,
                    button_released_color,
                    button_released_color,
                    button_released_color
                };

                if (remap_button_sel) colors[remap_button_sel_i] = button_pressed_color;

                renderMsgsCentered(p_msg, OPTIONS_MENU_BUTTONS_SIZE, hitbox, colors);
            }

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
                    if (remap_button_sel) {
                        remap_button_sel = false;
                        players[sel_player_i].bindings[remap_button_sel_i] = event.key.keysym.sym;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    struct Pos mouse_pos = {.x = event.button.x, .y = event.button.y};
                    bool pressed_button = false;
                    for (size_t i = 0; i < OPTIONS_MENU_BUTTONS_SIZE; i++) {
                        if (!remap_button_sel && rectContainsPos(&hitbox[i], &mouse_pos)) {
                            pressed_button = true;
                            remap_button_sel = true;
                            remap_button_sel_i = i;
                        } 
                    }
                    if (!pressed_button) {
                        remap_button_sel = false;
                    }

                    if (rectContainsPos(&sel_player_hitbox, &mouse_pos)) {
                        sel_player_i++;
                        if (sel_player_i >= players_size) {
                            sel_player_i = 0;
                        }
                    }
                } break;
                }
            }
        } break;
        case RUNNING: {
            bool all_died = true;
            for (size_t i = 0; i < players_size; i++) {
                if (!players[i].game_over) all_died = false;
            }

            if (all_died) {
                mode = GAME_OVER;
                game_over_start = curr_time;
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
                    for (size_t i = 0; i < players_size; i++) {
                        if (!players[i].game_over) {
                            playerOnInput(&players[i], event.key.keysym.sym);
                        }
                    }
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mode = MENU;
                    }
                break;
                }
            }

            // =============
            // Update
            // =============
            playersUpdate(players, players_size, curr_time, game.apples, game.apples_size);

            // =============
            // Render
            // =============
            // Background
            SDL_SetRenderDrawColor(renderer, 0x3C, 0xDF, 0xFF, 255);
            SDL_RenderClear(renderer);

            // Grid
            SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 255);
            SDL_Rect grid_rect = {.x = GRID_X0, .y = GRID_Y0, .w = GRID_DIMENS, .h = GRID_DIMENS};
            SDL_RenderFillRect(renderer, &grid_rect);

            // Body
            for (size_t i = 0; i < players_size; i++) {
                playerRenderBody(&players[i]);
            }

            // Apple
            SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 255);
            for (size_t i = 0; i < game.apples_size; i++) {
                SDL_Rect apple_rect = posToRect(&game.apples[i].pos);
                SDL_RenderFillRect(renderer, &apple_rect);
            }

            // Snake Head
            for (size_t i = 0; i < players_size; i++) {
                playerRenderHead(&players[i]);
            }

            // Score
            for (size_t i = 0; i < players_size; i++) {
                if (players[i].score > 999) {
                    fprintf(stderr, "Score too big!\n");
                    goto cleanup_media;
                }
            }

            renderPlayersScore(players, players_size);
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

            if (curr_time - game_over_start > game_over_delay) {
                mode = MENU;
                reset(&game, players, MAX_PLAYERS_SIZE);
            }

            SDL_Color font_color = {0xFF, 0x00, 0, 255};

            SDL_Surface* game_over_surf = TTF_RenderText_Solid(font, "Game Over", font_color);
            SDL_Texture* game_over_text = SDL_CreateTextureFromSurface(renderer, game_over_surf);
            SDL_FreeSurface(game_over_surf);

            SDL_Rect game_over_rect;
            TTF_SizeText(font, "Game Over", &game_over_rect.w, &game_over_rect.h);
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
