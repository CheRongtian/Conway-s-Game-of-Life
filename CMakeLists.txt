cmake_minimum_required(VERSION 3.15)
project(LifeLab LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)

add_executable(LifeLab
    Improved.c
    life.c
    glider.c
    pattern.c
    eater1.c
    outcome_analysis.c
    collision.c
    collision_search.c
    collision_replay.c
    collision_browser.c
    render.c
)

target_link_libraries(LifeLab PRIVATE SDL2::SDL2)
