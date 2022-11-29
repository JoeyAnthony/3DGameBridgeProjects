add_library(${TARGET_IMGUI} INTERFACE)
target_include_directories(${TARGET_IMGUI} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                                                        "$<INSTALL_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/imgui>")
