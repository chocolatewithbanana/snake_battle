#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 500
#define GRID_SIZE 20
#define BODY_SIZE (GRID_SIZE*GRID_SIZE)

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
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
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
    font = TTF_OpenFont("Ubuntu-Regular.ttf", 24);
    if (!font) {
        printSdlError("Loading Ubuntu-Regular.ttf");
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

SDL_Rect posToRect(struct Pos* p_pos) {
    SDL_Rect rect;
    rect.x = GRID_X0 + p_pos->x * GRID_DIMENS / GRID_SIZE;
    rect.y = GRID_Y0 + p_pos->y * GRID_DIMENS / GRID_SIZE;
    rect.w = GRID_DIMENS / GRID_SIZE;
    rect.h = GRID_DIMENS / GRID_SIZE;

    return rect;
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
    enum Direction direc = NONE;

    for (size_t i = 0; i < 4; i++) {
        if (p_player->bindings[i] == key) {
            direc = (enum Direction)i;
        }
    } 

    if (direc != NONE) {
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

void playerCheckMoveCollide(
    struct Player* p_player, uint32_t curr_time,
    struct Apple* apples, size_t apples_size
) {
    if (curr_time - p_player->last_movem_frame > p_player->movem_delay) {
        p_player->last_movem_frame = curr_time;
        p_player->reset_buffer_on_input = true;

        struct Pos last_pos = p_player->pos;

        // move head
        if (p_player->direc_i < p_player->direc_size) {
            p_player->direc = p_player->direc_buff[p_player->direc_i++];
        }

        switch (p_player->direc) {
            case UP:
                p_player->pos.y--;
            break;
            case DOWN:
                p_player->pos.y++;
            break;
            case LEFT:
                p_player->pos.x--;
            break;
            case RIGHT:
                p_player->pos.x++;
            break;
            case NONE:
                assert(false);
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

        // check apples
        for (size_t i = 0; i < apples_size; i++) {
            if (p_player->pos.x == apples[i].pos.x
                    && p_player->pos.y == apples[i].pos.y) {
                p_player->score++;
                p_player->body_size++;

                appleInit(&apples[i]);
            }
        }

        // move body
        if (p_player->body_size > 0) {
            for (size_t i = p_player->body_size-1; i > 0; i--) {
                p_player->body[i] = p_player->body[i-1]; 
            }
            p_player->body[0] = last_pos;
        }

        for (size_t i = 0; i < p_player->body_size; i++) {
            if (p_player->body[i].x == p_player->pos.x && p_player->body[i].y == p_player->pos.y) {
                p_player->game_over = true;
            }
        }
    }
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
        case UP:
            rotation = 0;
        break;
        case RIGHT:
            rotation = 90;
        break;
        case DOWN:
            rotation = 180;
        break;
        case LEFT:
            rotation = 270;
        break;
        case NONE:
            assert(false);
        break;
    }
    
    SDL_RenderCopyEx(renderer, head_text, NULL, &head_rect, rotation, NULL, 0);
}

int main() {
    if (!init()) {
        goto cleanup; 
    }

    if (!loadMedia()) {
        goto cleanup;
    }

    srand(time(NULL));

    struct Game game;
    gameInit(&game);

    struct Player p1;
    playerInit(&p1);
    
    p1.bindings[LEFT] = SDLK_LEFT;
    p1.bindings[RIGHT] = SDLK_RIGHT;
    p1.bindings[UP] = SDLK_UP;
    p1.bindings[DOWN] = SDLK_DOWN;

    struct Player p2;
    playerInit(&p2);

    p2.bindings[LEFT] = SDLK_a;
    p2.bindings[RIGHT] = SDLK_d;
    p2.bindings[UP] = SDLK_w;
    p2.bindings[DOWN] = SDLK_s;

    while (!p1.game_over && !p2.game_over) {
        uint32_t curr_time = SDL_GetTicks();

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
                    playerOnInput(&p1, event.key.keysym.sym);
                    playerOnInput(&p2, event.key.keysym.sym);
                break;
            }
        }

        playerCheckMoveCollide(&p1, curr_time, game.apples, game.apples_size);
        playerCheckMoveCollide(&p2, curr_time, game.apples, game.apples_size);

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
        playerRenderBody(&p1);
        playerRenderBody(&p2);

        // Apple
        SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 255);
        for (size_t i = 0; i < game.apples_size; i++) {
            SDL_Rect apple_rect = posToRect(&game.apples[i].pos);
            SDL_RenderFillRect(renderer, &apple_rect);
        }

        // Snake Head
        playerRenderHead(&p1);
        playerRenderHead(&p2);

        /*
        // Score
        if (player.score > 999) {
            printf("Score muito grande");
            goto cleanup_media;
        }
        // 3 bytes for numbers
        char score_message[15] = "Pontuacao: ";

        sprintf(score_message, "Pontuacao: %d", player.score);
        SDL_Color score_color = {0x56, 0x73, 0x45, 255};
        SDL_Surface* score_surf = TTF_RenderText_Solid(font, score_message, score_color);
        SDL_Texture* score_text = SDL_CreateTextureFromSurface(renderer, score_surf);

        SDL_FreeSurface(score_surf);

        SDL_Rect score_rect;
        TTF_SizeText(font, "Game Over", &score_rect.w, &score_rect.h);
        score_rect.x = 0;
        score_rect.y = 0;
        SDL_RenderCopy(renderer, score_text, NULL, &score_rect);

        SDL_DestroyTexture(score_text);
        */

        // Render to screen
        SDL_RenderPresent(renderer);

        SDL_Delay(1000 / 60);
    }

    // Game Over
    SDL_Color font_color = {0xFF, 0x00, 0, 255};

    SDL_Surface* game_over_surf = TTF_RenderText_Solid(font, "Game Over", font_color);
    SDL_Texture* game_over_text = SDL_CreateTextureFromSurface(renderer, game_over_surf);
    SDL_FreeSurface(game_over_surf);

    SDL_Rect game_over_rect;
    TTF_SizeText(font, "Game Over", &game_over_rect.w, &game_over_rect.h);
    game_over_rect.x = WINDOW_WIDTH/2 - game_over_rect.w/2;
    game_over_rect.y = WINDOW_HEIGHT/2 - game_over_rect.h/2;
    SDL_RenderCopy(renderer, game_over_text, NULL, &game_over_rect);
    SDL_RenderPresent(renderer);

    SDL_Delay(3000);

    SDL_DestroyTexture(game_over_text);

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
