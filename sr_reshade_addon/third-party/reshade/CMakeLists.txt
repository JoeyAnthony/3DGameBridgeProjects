add_library(${TARGET_RESHADE} INTERFACE)
target_include_directories(${TARGET_RESHADE} INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
