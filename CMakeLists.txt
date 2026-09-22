cmake_minimum_required(VERSION 3.15)
project(PDFView VERSION 0.2 LANGUAGES C)

include(GenerateG3A)
find_package(Gint 2.9 REQUIRED)

set(SOURCES
    src/main.c
)

add_executable(pdfview ${SOURCES})
target_compile_options(pdfview PRIVATE -Wall -Wextra -Os)
target_link_libraries(pdfview Gint::Gint)

if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
    # Empty internal name improves compatibility with some recent fx-CG50 OS versions.
    generate_g3a(TARGET pdfview OUTPUT "PDFView.g3a"
        NAME "" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
