cmake_minimum_required(VERSION 3.16)
project(mixxtape_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components)

add_library(tape_core STATIC
    ${CORE_DIR}/tape/src/tape_store.c
    ${CORE_DIR}/tape/src/sbc_frame.c
    ${CORE_DIR}/tape/src/adpcm.c
    ${CORE_DIR}/tape/src/adpcm_block.c
    ${CORE_DIR}/tape/src/tape_player.c
    ${CORE_DIR}/leds/src/reels.c
    ${CORE_DIR}/factory/src/factory_test.c
    ${CORE_DIR}/bt/src/pairing.c
    ${CORE_DIR}/bt/src/bonds.c
    ${CORE_DIR}/ui/src/ui_state.c
)
target_include_directories(tape_core PUBLIC
    ${CORE_DIR}/tape/include
    ${CORE_DIR}/ui/include
    ${CORE_DIR}/leds/include
    ${CORE_DIR}/factory/include
    ${CORE_DIR}/bt/include
)

if(MSVC)
    target_compile_options(tape_core PRIVATE /W4)
else()
    target_compile_options(tape_core PRIVATE -Wall -Wextra -Wshadow)
endif()

add_library(mock_flash STATIC mock_flash.c)
target_link_libraries(mock_flash PUBLIC tape_core)
target_include_directories(mock_flash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

foreach(t test_tape_store test_sbc_frame test_adpcm test_adpcm_block
            test_tape_player test_reels test_ui_state test_factory_test
            test_pairing test_bonds)
    add_executable(${t} ${t}.c)
    target_link_libraries(${t} PRIVATE tape_core mock_flash)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
