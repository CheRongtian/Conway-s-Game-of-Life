// Improved.c - SDL2 window version

#include <stdint.h>
#include <SDL.h>

#define WIDTH 512
#define HEIGHT 256

#define CELL_SIZE 2
#define DELAY_MS 50

static uint8_t board_a[HEIGHT + 2][WIDTH + 2];
static uint8_t board_b[HEIGHT + 2][WIDTH + 2];

static void sync_border(uint8_t board[HEIGHT + 2][WIDTH + 2])
{
    for (int y = 1; y <= HEIGHT; ++y) {
        board[y][0] = board[y][WIDTH];
        board[y][WIDTH + 1] = board[y][1];
    }

    for (int x = 1; x <= WIDTH; ++x) {
        board[0][x] = board[HEIGHT][x];
        board[HEIGHT + 1][x] = board[1][x];
    }

    board[0][0] = board[HEIGHT][WIDTH];
    board[0][WIDTH + 1] = board[HEIGHT][1];
    board[HEIGHT + 1][0] = board[1][WIDTH];
    board[HEIGHT + 1][WIDTH + 1] = board[1][1];
}

static void step(
    uint8_t current[HEIGHT + 2][WIDTH + 2],
    uint8_t next[HEIGHT + 2][WIDTH + 2])
{
    sync_border(current);

    for (int y = 1; y <= HEIGHT; ++y) {
        for (int x = 1; x <= WIDTH; ++x) {
            int n =
                current[y - 1][x - 1] +
                current[y - 1][x] +
                current[y - 1][x + 1] +
                current[y][x - 1] +
                current[y][x + 1] +
                current[y + 1][x - 1] +
                current[y + 1][x] +
                current[y + 1][x + 1];

            next[y][x] =
                (n == 3) || (current[y][x] && n == 2);
        }
    }
}

static void render(
    SDL_Renderer *renderer,
    uint8_t board[HEIGHT + 2][WIDTH + 2])
{
    SDL_SetRenderDrawColor(renderer, 5, 8, 15, 255);
    SDL_RenderClear(renderer);

    for (int y = 1; y <= HEIGHT; ++y) {
        for (int x = 1; x <= WIDTH; ++x) {
            if (board[y][x]) {
                SDL_SetRenderDrawColor(renderer, 255, 80, 120, 255);

                SDL_Rect cell = {
                    (x - 1) * CELL_SIZE,
                    (y - 1) * CELL_SIZE,
                    CELL_SIZE,
                    CELL_SIZE
                };

                SDL_RenderFillRect(renderer, &cell);
            }
            else {
                SDL_SetRenderDrawColor(renderer, 30, 45, 70, 255);

                SDL_RenderDrawPoint(
                    renderer,
                    (x - 1) * CELL_SIZE + CELL_SIZE / 2,
                    (y - 1) * CELL_SIZE + CELL_SIZE / 2
                );
            }
        }
    }

    SDL_RenderPresent(renderer);
}

int main(void)
{
    uint8_t (*current)[WIDTH + 2] = board_a;
    uint8_t (*next)[WIDTH + 2] = board_b;

    current[1][2] = 1;
    current[2][3] = 1;
    current[3][1] = 1;
    current[3][2] = 1;
    current[3][3] = 1;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "Conway's Game of Life",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH * CELL_SIZE,
        HEIGHT * CELL_SIZE,
        0
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
        }

        render(renderer, current);

        step(current, next);

        uint8_t (*tmp)[WIDTH + 2] = current;
        current = next;
        next = tmp;

        SDL_Delay(DELAY_MS);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}