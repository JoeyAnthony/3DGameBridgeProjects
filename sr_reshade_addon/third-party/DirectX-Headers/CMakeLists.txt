add_library(${TARGET_DXHEADERS} INTERFACE)
target_include_directories(${TARGET_DXHEADERS} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                                                        "$<INSTALL_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/DirectX-Headers/include>")
