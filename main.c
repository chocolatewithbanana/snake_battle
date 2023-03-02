#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

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

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL; 
SDL_Texture* head_text = NULL;
SDL_Texture* body_text = NULL;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define GRID_DIMENS MIN(WINDOW_WIDTH, WINDOW_HEIGHT)
#define GRID_X0 (WINDOW_WIDTH / 2 - GRID_DIMENS / 2)
#define GRID_Y0 (WINDOW_HEIGHT / 2 - GRID_DIMENS / 2)

#define return_defer(x) do {ret = x; goto defer;} while(0)

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
    fprintf(stderr, "Error ");
    fprintf(stderr, "%s", message);
    fprintf(stderr, ": %s\n", SDL_GetError());
}

void errnoAbort(char* message) {
    perror(message);
    exit(-1);
}

// posix check error
int pcr(int ret, char* message) {
    if (ret < 0) errnoAbort(message);

    return ret;
}

// posix check pointer
void* pcp(void* p, char* message) {
    if (!p) errnoAbort(message);

    return p;
}

bool init() {
    bool ret = true;

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) return_defer(false);
    if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) return_defer(false);
    if (TTF_Init() != 0) return_defer(false);
        
    window = SDL_CreateWindow("Snake Game",
        0, 0,
        WINDOW_WIDTH, WINDOW_HEIGHT, 0
    );
    if (!window) return_defer(false);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return_defer(false);

defer:
    if (!ret) printSdlError("init");
    return ret;
}

bool loadMedia() {
    bool ret = true;

    font = TTF_OpenFont("COMIC.TTF", 24);
    if (!font) return_defer(false);

    SDL_Surface* head_surf = IMG_Load("head.png");
    if (!head_surf) return_defer(false);

    head_text = SDL_CreateTextureFromSurface(renderer, head_surf);
    SDL_FreeSurface(head_surf);

    if (!head_text) return_defer(false);

    SDL_Surface* body_surf = IMG_Load("body.png");
    if (!body_surf) return_defer(false);

    body_text = SDL_CreateTextureFromSurface(renderer, body_surf);
    SDL_FreeSurface(body_surf);

    if (!body_text) return_defer(false);

defer:
    if (!ret) printSdlError("load media");
    return ret;
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

bool mapKeycode(SDL_Keycode* bindings, SDL_Keycode keycode, enum Direction* direc) {
    for (size_t i = 0; i < 4; i++) {
        if (bindings[i] == keycode) {
            *direc = (enum Direction)i;
            return true;
        }
    }

    return false;
}

void addDirection(struct Player* player, enum Direction direc) {
    if (player->reset_buffer_on_input) {
        player->reset_buffer_on_input = false;
        player->direc_size = 0;
        player->direc_i = 0;
    }
    if (player->direc_size < DIREC_BUFFER_SIZE) {
        player->direc_buff[player->direc_size++] = direc;
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
        case DOWN: {
            game_state->players[i].pos.y++;
        } break;
        case LEFT: {
            game_state->players[i].pos.x--;
        } break;
        case RIGHT: {
            game_state->players[i].pos.x++;
        } break;
        case UP: {
            game_state->players[i].pos.y--;
        } break;
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

    assert(y >= 0);

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
    case DOWN: {
        rotation = 180;
    } break;
    case RIGHT: {
        rotation = 90;
    } break;
    case LEFT: {
        rotation = 270;
    } break;
    case UP: {
        rotation = 0;
    } break;
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
    RUNNING,   
    MENU,
    GAME_OVER,
    QUIT_GAME
};

enum MenuMode {
    MN_CHOOSE_NETWORK,
    MN_HOST_OR_JOIN,
    MN_LOBBY,
    MN_REMAP_MENU,
    MN_OPTIONS_MENU,
    MN_MAIN_MENU
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

void clearScreen() {
    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
    SDL_RenderClear(renderer);
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

bool readBytes(int fd, void* data, size_t data_size, bool wait) {
    assert(data_size != 0);

    while (true) {
        ssize_t bytes = recv(fd, data, data_size, MSG_PEEK); 
        assert(bytes <= (ssize_t)data_size);

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (wait) continue;
                else return false;
            } else {
                errnoAbort("Read failed");
            }
        }
        
        if (bytes == 0) {
            fprintf(stderr, "Disconnected\n");
            exit(-1);
        }
        
        read(fd, data, data_size);
        return true;
    }
}

void writeBytes(int fd, void* data, size_t data_size) {
    int ret = send(fd, data, data_size, MSG_NOSIGNAL);
    pcr(ret, "Write error");
}

struct Input {
    struct Pos mouse_pos;
    bool is_mouse_clicked;
    bool is_key_pressed;
    SDL_Keycode key_pressed;
    bool letters_pressed[26];
    bool arrows_pressed[4];
};

bool validKey(SDL_Keycode key) {
    return (SDLK_a <= key && key <= SDLK_z)
        || (SDLK_RIGHT <= key && key <= SDLK_UP);
}

bool getInput(struct Input* input) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT: {
            return false;
        } break;
        case SDL_MOUSEBUTTONDOWN: {
            input->mouse_pos.x = event.button.x;
            input->mouse_pos.y = event.button.y;
            input->is_mouse_clicked = true;
        } break;
        case SDL_KEYDOWN: {
            SDL_Keycode key = event.key.keysym.sym;
            if (SDLK_a <= key && key <= SDLK_z) {
                input->letters_pressed[key - SDLK_a] = true;
            } else if (SDLK_RIGHT <= key && key <= SDLK_UP) {
                input->arrows_pressed[key - SDLK_RIGHT] = true;
            }
            input->is_key_pressed = true;
            input->key_pressed = key;
        } break;
        }
    }

    return true;
}


// @todo local variables?
// Init game structs
struct GameState game_state = {
    .players_size = 1,
    .dead_bodies_size = 0,
};

enum Mode mode = MENU;
enum MenuMode menu_mode = MN_CHOOSE_NETWORK;
bool already_running = false;

struct GameOver {
    uint32_t delay;
    uint32_t start;
} game_over = {
    .delay = 1000,
};

struct Lobby {
    uint16_t delay;
    uint16_t start;
} lobby = {
    .delay = 100,
    .start = 0
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

struct Network {
    struct sockaddr_in host_addr;
} network;

struct NetworkHost {
    int fd[MAX_PLAYERS_SIZE];
} host;

struct NetworkClient {
    int fd;
    size_t player_i;
} client;

bool is_online = false;
bool is_host = false;

uint32_t curr_time; 

struct Input input;

void getAddrAndPort(struct in_addr* addr, uint16_t* port) {
    // Get IP
    // @todo get max(ipv4.len, ipv6.len)
    while (true) {
        printf("Enter the IP address: ");
        char ip[INET_ADDRSTRLEN];

        char format[6] = " %00s";
        sprintf(format, " %%%ds", INET_ADDRSTRLEN-1);

        int result = scanf(format, ip);

        if (result != 1) {
            while (getchar() != '\n') {}
            printf("Invalid input\n");
            continue;
        }

        result = inet_pton(AF_INET, ip, addr);
        pcr(result, "inet_pton failed");

        if (result == 0) {
            printf("Invalid IP\n");     
            continue;
        }

        break;
    }

    // Get port
    while (true) {
        printf("Choose a port: ");                                  

        // @todo ub in case of overflow
        int res = scanf("%hu", port);

        if (res != 1 || *port < 1024) {
            printf("Invalid port\n");
        } else {
            break;
        }
    }
}

void block(int fd) {
    pcr(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK),
        "Error setting O_NONBLOCK"
       );
}

void unblock(int fd) {
    pcr(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK),
        "Error setting O_NONBLOCK"
       );
}

bool runMenu() {
    clearScreen();

    switch (menu_mode) {
    case MN_CHOOSE_NETWORK: {
        enum {BUTTONS_QTY = 3};
        char* msgs[BUTTONS_QTY] = {"Local", "Online", "Quit"};
        enum Options {LOCAL, ONLINE, QUIT};

        SDL_Rect hitboxes[BUTTONS_QTY];

        SDL_Color colors[BUTTONS_QTY];
        for (size_t i = 0; i < BUTTONS_QTY; i++) {
            colors[i] = menu.button_color;
        }

        renderMsgsCentered(msgs, BUTTONS_QTY, hitboxes, colors);

        if (input.is_mouse_clicked) {
            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                if (rectContainsPos(&hitboxes[i], &input.mouse_pos)) {
                    switch ((enum Options)i) {
                    case LOCAL: {
                        is_online = false;
                        menu_mode = MN_MAIN_MENU;
                    } break; 
                    case ONLINE: {
                        is_online = true;
                        menu_mode = MN_HOST_OR_JOIN;
                    } break;
                    case QUIT: {
                        mode = QUIT_GAME;
                    } break;
                    }  
                } 
            }
        }
    } break;
    case MN_HOST_OR_JOIN: {
        // @todo close sockets
        // @todo currently read/write ignores endianness

        enum {BUTTONS_QTY = 2};
        char* msgs[BUTTONS_QTY] = {"Host", "Join"};
        enum HostOrJoin {HOST, JOIN};

        SDL_Rect hitboxes[BUTTONS_QTY];

        SDL_Color colors[BUTTONS_QTY];
        for (size_t i = 0; i < BUTTONS_QTY; i++) {
            colors[i] = menu.button_color;
        }

        renderMsgsCentered(msgs, BUTTONS_QTY, hitboxes, colors);

        if (input.is_mouse_clicked) {
            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                if (rectContainsPos(&hitboxes[i], &input.mouse_pos)) {
                    struct in_addr addr;
                    uint16_t port;
                    getAddrAndPort(&addr, &port);

                    // @todo check if it is linux
                    // Init socket
                    int fd = socket(AF_INET, SOCK_STREAM, 0);
                    pcr(fd, "Socket creation failed");

                    memset(&network.host_addr, 0, sizeof(network.host_addr));
                    network.host_addr.sin_family = AF_INET;
                    network.host_addr.sin_port = htons(port);
                    network.host_addr.sin_addr = addr;

                    // Bind/Connect
                    switch ((enum HostOrJoin)i) {
                    case HOST: {
                        is_host = true;

                        host.fd[0] = fd;
                        game_state.players_size = 1;

                        unblock(host.fd[0]);

                        // @todo use select or poll?
                        pcr(bind(host.fd[0], (struct sockaddr*)&network.host_addr, sizeof(network.host_addr)),
                                "Bind failed"
                           );

                        pcr(listen(host.fd[0], MAX_PLAYERS_SIZE),
                                "Listening failed"
                           );

                        printf("Listening\n");
                    } break;
                    case JOIN: {
                        is_host = false;

                        client.fd = fd;

                        while (true) {
                            // Original: if errno == EINPROGRESS || connect >= 0 break
                            int result = connect(fd, (struct sockaddr*) &network.host_addr, sizeof(network.host_addr));

                            if (result < 0) {
                                if (errno == EINPROGRESS) continue;
                                else errnoAbort("Connection failed");
                            }

                            break;
                        }
                        printf("Connected\n");
                        unblock(client.fd);

                        readBytes(client.fd, &client.player_i, sizeof(client.player_i), true);
                    } break;
                    }

                    menu_mode = MN_LOBBY;
                }      
            }
        }
    } break;
    case MN_LOBBY: {
        enum Type {
            START_GAME,
            UPDATE, 
        };

        struct Packet {
            enum Type type;
            size_t update_players_size;
        };

        if (is_host) {
            socklen_t len = sizeof(network.host_addr);
            int fd = accept(host.fd[0], (struct sockaddr*) &network.host_addr, &len);
            if (fd >= 0) {
                writeBytes(fd, &game_state.players_size, sizeof(game_state.players_size));

                pcr(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK),
                        "Error setting O_NONBLOCK"
                   );

                host.fd[game_state.players_size++] = fd;
            }

            if (curr_time > lobby.start + lobby.delay) {
                lobby.start = curr_time;
                for (size_t i = 1; i < game_state.players_size; i++) {
                    struct Packet packet = {.type = UPDATE, .update_players_size = game_state.players_size};
                    writeBytes(host.fd[i], &packet, sizeof(packet));
                }
            }
        } else {
            while (true) {
                struct Packet packet;
                if (!readBytes(client.fd, &packet, sizeof(packet), false)) {
                    break;
                }

                switch (packet.type) {
                case UPDATE: {
                    game_state.players_size = packet.update_players_size;
                } break;
                case START_GAME: {
                    mode = RUNNING;
                } break;
                }
            }
        }

        // Render buttons
        enum {BUTTONS_QTY = MAX_PLAYERS_SIZE + 1};
        enum {READY_BUTTON = MAX_PLAYERS_SIZE};

        char connect_info[MAX_PLAYERS_SIZE][24];
        for (size_t i = 0; i < MAX_PLAYERS_SIZE; i++) {
            strcpy(connect_info[i], "Player 0: Not connected");
            connect_info[i][7] = '1' + i;
        }
        for (size_t i = 0; i < game_state.players_size; i++) {
            strcpy(&connect_info[i][10], "Connected");
        }

        char* msgs[MAX_PLAYERS_SIZE+1];
        for (size_t i = 0; i < MAX_PLAYERS_SIZE; i++) {
            msgs[i] = connect_info[i];
        }
        msgs[MAX_PLAYERS_SIZE] = "Ready";

        SDL_Rect hitboxes[MAX_PLAYERS_SIZE+1];

        SDL_Color colors[MAX_PLAYERS_SIZE+1];
        for (size_t i = 0; i < MAX_PLAYERS_SIZE+1; i++) {
            colors[i] = menu.button_color;
        }

        renderMsgsCentered(msgs, MAX_PLAYERS_SIZE+1, hitboxes, colors);

        if (input.is_mouse_clicked) {
            if (is_host) {
                if (rectContainsPos(&hitboxes[READY_BUTTON], &input.mouse_pos)) {
                    mode = RUNNING;
                    for (size_t i = 1; i < game_state.players_size; i++) {
                        struct Packet packet = {.type = START_GAME};
                        writeBytes(host.fd[i], &packet, sizeof(packet));
                    }
                }
            }
        }
    } break;
    case MN_MAIN_MENU: {
        enum {BUTTONS_QTY = 3};
        enum Button {M_START, M_OPTIONS, M_BACK};
        char* msg[BUTTONS_QTY] = {"Start", "Options", "Back"};

        if (already_running) {
            msg[0] = "Continue";
        }

        SDL_Color colors[BUTTONS_QTY]; 
        for (size_t i = 0; i < BUTTONS_QTY; i++) {
            colors[i] = menu.button_color;                
        }

        SDL_Rect hitboxes[BUTTONS_QTY];
        renderMsgsCentered(msg, BUTTONS_QTY, hitboxes, colors);

        if (input.is_mouse_clicked) {
            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                if (rectContainsPos(&hitboxes[i], &input.mouse_pos)) {
                    switch ((enum Button)i) {
                    case M_START: {
                        mode = RUNNING;
                        already_running = true;
                    } break;
                    case M_OPTIONS: {
                        menu_mode = MN_OPTIONS_MENU;
                    } break;
                    case M_BACK: {
                        menu_mode = MN_CHOOSE_NETWORK;
                        reset(&game_state);
                        already_running = false;
                    } break;
                    }
                }
            }
        }

        if (input.is_key_pressed && input.key_pressed == SDLK_ESCAPE) {
            if (already_running) {
                mode = RUNNING;
            } else {
                menu_mode = MN_CHOOSE_NETWORK;
            }
        }
    } break;
    case MN_REMAP_MENU: {
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

        enum {BUTTONS_QTY = 4};

        SDL_Rect hitbox[BUTTONS_QTY];
        {
            char msg[BUTTONS_QTY][9];
            // It has to be in this order (bindings order)
            char* pre_msg[BUTTONS_QTY] = {"DOWN: 0", "LEFT: 0", "RIGHT: 0", "UP: 0"};

            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                strcpy(msg[i], pre_msg[i]);
            }

            for (size_t i = 0; i < BUTTONS_QTY; i++) {
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

            char* p_msg[BUTTONS_QTY];
            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                p_msg[i] = msg[i]; 
            }

            SDL_Color colors[BUTTONS_QTY];

            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                colors[i] = menu.button_color;
            }

            if (remap_menu.button_sel) {
                colors[remap_menu.button_sel_i] = remap_menu.button_sel_color;
            }

            renderMsgsCentered(p_msg, BUTTONS_QTY, hitbox, colors);
        }

        // Events
        if (input.is_mouse_clicked) {
            bool pressed_button = false;
            for (size_t i = 0; i < BUTTONS_QTY; i++) {
                if (!remap_menu.button_sel && rectContainsPos(&hitbox[i], &input.mouse_pos)) {
                    pressed_button = true;
                    remap_menu.button_sel = true;
                    remap_menu.button_sel_i = i;
                } 
            }
            if (!pressed_button) {
                remap_menu.button_sel = false;
            }

            if (rectContainsPos(&sel_player_hitbox, &input.mouse_pos)) {
                remap_menu.sel_player_i++;
                if (remap_menu.sel_player_i >= game_state.players_size) {
                    remap_menu.sel_player_i = 0;
                }
            }
        }
        if (input.is_key_pressed) {
            if (input.is_key_pressed && input.key_pressed == SDLK_ESCAPE) {
                menu_mode = MN_OPTIONS_MENU;
            } else if (remap_menu.button_sel && validKey(input.key_pressed)) {
                game_state.players[remap_menu.sel_player_i].bindings[remap_menu.button_sel_i] = input.key_pressed;
                remap_menu.button_sel = false;
            }
        } 
    } break;
    case MN_OPTIONS_MENU: {
        enum {BUTTONS_SIZE = 2};

        enum Options {PLAYERS, REMAP};

        // It has to be in this order
        char* pre_msg[BUTTONS_SIZE] = {
            "Players: 0",
            "Remap"
        };
        char msg[BUTTONS_SIZE][11];

        for (size_t i = 0; i < BUTTONS_SIZE; i++) {
            strcpy(msg[i], pre_msg[i]);
        }

        size_t len = strlen(msg[PLAYERS]);
        msg[PLAYERS][len-1] = game_state.players_size + '0';

        char* p_msg[BUTTONS_SIZE];
        for (size_t i = 0; i < BUTTONS_SIZE; i++) {
            p_msg[i] = msg[i]; 
        }

        SDL_Color colors[BUTTONS_SIZE];
        for (size_t i = 0; i < BUTTONS_SIZE; i++) {
            colors[i] = menu.button_color;
        }

        SDL_Rect hitbox[BUTTONS_SIZE];

        renderMsgsCentered(p_msg, BUTTONS_SIZE, hitbox, colors);

        if (input.is_key_pressed) {
            if (input.is_key_pressed && input.key_pressed == SDLK_ESCAPE) {
                menu_mode = MN_MAIN_MENU;
            }
            if (remap_menu.button_sel) {
                remap_menu.button_sel = false;
                game_state.players[remap_menu.sel_player_i].bindings[remap_menu.button_sel_i] = input.key_pressed;
            }
        }
        if (input.is_mouse_clicked) {
            if (rectContainsPos(&hitbox[PLAYERS], &input.mouse_pos)) {
                game_state.players_size++;
                if (game_state.players_size > MAX_PLAYERS_SIZE) {
                    game_state.players_size = 1;
                }
            }
            if (rectContainsPos(&hitbox[REMAP], &input.mouse_pos)) {
                menu_mode = MN_REMAP_MENU;
            }
        }
    } break;
    }

    return true;
}

void runRunning() {
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

    // Get online directions
    if (is_online && is_host) {
        for (size_t i = 1; i < game_state.players_size; i++) {
            while (true) {
                enum Direction direc;
                if (!readBytes(host.fd[i], &direc, sizeof(direc), false)) {
                    break;
                }
                addDirection(&game_state.players[i], direc);
            }
        }
    }

    // Events
    if (input.key_pressed) {
        if (is_online) {
            if (is_host) {
                enum Direction direc;
                if (mapKeycode(game_state.players[0].bindings, input.key_pressed, &direc)) {
                    addDirection(&game_state.players[0], direc);
                }
            } else {
                enum Direction direc;
                if (mapKeycode(game_state.players[client.player_i].bindings, input.key_pressed, &direc)) {
                    writeBytes(client.fd, &direc, sizeof(direc));
                }
            }
        } else {
            if (input.key_pressed == SDLK_ESCAPE) {
                mode = MENU;
            } else {
                for (size_t i = 0; i < game_state.players_size; i++) {
                    if (!game_state.players[i].game_over) {
                        enum Direction direc;
                        if (mapKeycode(game_state.players[i].bindings, input.key_pressed, &direc)) {
                            addDirection(&game_state.players[i], direc);
                        }
                    }
                }
            }
        }
    }

    // Update
    if (!(is_online && !is_host)) {
        gameStateUpdate(&game_state, curr_time);
    }

    if (is_online) {
        if (is_host) {
            for (size_t i = 1; i < game_state.players_size; i++) {
                writeBytes(host.fd[i], &game_state, sizeof(game_state));
            }
        } else {
            while (true) {
                struct GameState temp;
                if (!readBytes(client.fd, &temp, sizeof(temp), false)) {
                    break;
                }
                game_state = temp;
            }
        }
    } 

    // Render
    for (size_t i = 0; i < game_state.players_size; i++) {
        if (game_state.players[i].score > 999) {
            fprintf(stderr, "Score too big!\n");
            exit(-1);
        }
    }

    render(&game_state);
}

void runGameOver() {
    if (curr_time - game_over.start > game_over.delay) {
        mode = MENU;
        menu_mode = MN_MAIN_MENU;
        reset(&game_state);
    }

    bool draw = false;
    int max_score = 0;
    size_t winner;
    for (size_t i = 0; i < game_state.players_size; i++) {
        if (i == 0) {
            draw = false;
            max_score = game_state.players[i].score;
            winner = 0;
        }
        else if (game_state.players[i].score == max_score) {
            draw = true;
        } else if (game_state.players[i].score > max_score) {
            draw = false;
            max_score = game_state.players[i].score;
            winner = i;
        }     
    }

    char msg[14];

    if (draw) {      
        strcpy(msg, "Draw");
    } else {
        strcpy(msg, "Player 0 wins"); 
        msg[7] = winner + 1 + '0';
    }

    SDL_Rect game_over_rect;
    TTF_SizeText(font, msg, &game_over_rect.w, &game_over_rect.h);
    game_over_rect.x = WINDOW_WIDTH/2 - game_over_rect.w/2;
    game_over_rect.y = WINDOW_HEIGHT/2 - game_over_rect.h/2;

    renderMsg(msg, &game_over_rect, menu.button_color);
}

int main() {
    int ret = 0;

    srand(time(NULL));

    if (!init()) {
        return_defer(-1);
    }

    if (!loadMedia()) {
        return_defer(-1);
    }

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

    // Main switch
    while (true) {
        curr_time = SDL_GetTicks();

        memset(&input, 0, sizeof(input));
        if (!getInput(&input)) {
            return_defer(0);
        }

        switch (mode) {
        case MENU: {
            if (!runMenu()) {
                return_defer(-1);
            }
        } break;
        case RUNNING: {
            runRunning();
        } break;
        case GAME_OVER: {
            runGameOver();
        } break;
        case QUIT_GAME: {
            return_defer(0);
        } break;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(1000 / 60);
    }

defer:
    if (body_text) SDL_DestroyTexture(body_text);
    if (head_text) SDL_DestroyTexture(head_text);
    if (font) TTF_CloseFont(font);

    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return ret;
}
