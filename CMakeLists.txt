cmake_minimum_required(VERSION 3.26)
project(010-movieinfo-hook VERSION 0.1.0.0 LANGUAGES CXX)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME               safetyhook
    GIT_TAG            v0.3.0
    GITHUB_REPOSITORY  cursey/safetyhook
    OPTIONS            "SAFETYHOOK_FETCH_ZYDIS ON"
)

file(GLOB_RECURSE VERSIONS "versions/*.cmake")
file(READ cmake/VersionOffsets.in VERSION_OFFSET_TEMPLATE)

foreach (VERSION_PATH ${VERSIONS})
    message(STATUS "Including game support file: ${VERSION_PATH}...")

    include(${VERSION_PATH})

    string(CONFIGURE ${VERSION_OFFSET_TEMPLATE} VERSION_OFFSET_TEXT @ONLY)
    string(APPEND VERSION_ALL_OFFSETS "${VERSION_OFFSET_TEXT}")
endforeach ()

configure_file(cmake/VersionHeader.in macros/versions.h @ONLY)

add_library(${PROJECT_NAME} SHARED
    src/main.cc
    src/avs2-log.h
    src/offsets.h
    src/parse_http_link.h
    src/parse_http_link.cc
)

target_link_directories(${PROJECT_NAME} PRIVATE lib)
target_link_libraries(${PROJECT_NAME} safetyhook avs2-core winhttp)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/macros
)