#include <stdio.h>
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
    RIGHT
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

    struct Player player;
    playerInit(&player);

    while (!player.game_over) {
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
                case SDL_KEYDOWN: {
                    enum Direction curr_direc;
                    bool pressed = false;
                    switch (event.key.keysym.sym) {
                        case SDLK_a:
                            curr_direc = LEFT;
                            pressed = true;
                        break;
                        case SDLK_s:
                            curr_direc = DOWN;
                            pressed = true;
                        break;
                        case SDLK_d:
                            curr_direc = RIGHT;
                            pressed = true;
                        break;
                        case SDLK_w:
                            curr_direc = UP;
                            pressed = true;
                        break;
                    } 
                    if (pressed) {
                        if (player.reset_buffer_on_input) {
                            player.reset_buffer_on_input = false;
                            player.direc_size = 0;
                            player.direc_i = 0;
                        }
                        if (player.direc_size < DIREC_BUFFER_SIZE) {
                            player.direc_buff[player.direc_size++] = curr_direc;
                        }
                    }
                } break;
            }
        }


        if (curr_time - player.last_movem_frame > player.movem_delay) {
            player.last_movem_frame = curr_time;
            player.reset_buffer_on_input = true;

            struct Pos last_pos = player.pos;

            // Move
            if (player.direc_i < player.direc_size) {
                player.direc = player.direc_buff[player.direc_i++];
            }

            switch (player.direc) {
                case UP:
                    player.pos.y--;
                break;
                case DOWN:
                    player.pos.y++;
                break;
                case LEFT:
                    player.pos.x--;
                break;
                case RIGHT:
                    player.pos.x++;
                break;
            }

            if (player.pos.x < 0) {
                player.pos.x = GRID_SIZE-1;
            } else if (player.pos.x >= GRID_SIZE) {
                player.pos.x = 0;
            }

            if (player.pos.y < 0) {
                player.pos.y = GRID_SIZE-1;
            } else if (player.pos.y >= GRID_SIZE) {
                player.pos.y = 0;
            }

            // Check colision
            for (size_t i = 0; i < game.apples_size; i++) {
                if (player.pos.x == game.apples[i].pos.x
                        && player.pos.y == game.apples[i].pos.y) {
                    player.score++;
                    player.body_size++;

                    appleInit(&game.apples[i]);
                }
            }

            // Move body
            if (player.body_size > 0) {
                for (size_t i = player.body_size-1; i > 0; i--) {
                    player.body[i] = player.body[i-1]; 
                }
                player.body[0] = last_pos;
            }

            for (size_t i = 0; i < player.body_size; i++) {
                if (player.body[i].x == player.pos.x && player.body[i].y == player.pos.y) {
                    player.game_over = true;
                }
            }
        }

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
        for (size_t i = 0; i < player.body_size; i++) {
            SDL_Rect body_rect = posToRect(&player.body[i]);
            SDL_RenderCopy(renderer, body_text, NULL, &body_rect);
        }

        // Apple
        SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 255);
        for (size_t i = 0; i < game.apples_size; i++) {
            SDL_Rect apple_rect = posToRect(&game.apples[i].pos);
            SDL_RenderFillRect(renderer, &apple_rect);
        }

        // Snake Head
        SDL_Rect head_rect = posToRect(&player.pos);

        double rotation;
        switch (player.direc) {
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
        }
        
        SDL_RenderCopyEx(renderer, head_text, NULL, &head_rect, rotation, NULL, 0);

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
