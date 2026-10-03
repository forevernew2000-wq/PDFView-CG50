cmake_minimum_required(VERSION 3.15)
project(ProyCalc VERSION 0.1 LANGUAGES C)

include(GenerateG3A)
find_package(Gint 2.9 REQUIRED)

add_executable(proycalc
    src/main.c
)

target_compile_options(proycalc PRIVATE -Wall -Wextra -Os)
target_link_libraries(proycalc Gint::Gint)

if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
    generate_g3a(TARGET proycalc OUTPUT "ProyCalc.g3a"
        NAME "" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
