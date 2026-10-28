add_library(${TARGET_OPENXR} INTERFACE)
target_include_directories(${TARGET_OPENXR} INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/OpenXR-SDK/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/dxr>"
)
