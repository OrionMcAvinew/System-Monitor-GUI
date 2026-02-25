cmake_minimum_required(VERSION 3.16)
project(SystemMonitorGUI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
option(BUILD_GUI "Build GUI executable" ON)

if(BUILD_GUI)
    find_package(OpenGL REQUIRED)

    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    add_subdirectory(external/glfw)

    set(IMGUI_SOURCES
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl3.cpp
        external/implot/implot.cpp
        external/implot/implot_items.cpp
        external/implot/implot_demo.cpp
    )

    add_executable(SystemMonitor
        src/main.cpp
        src/ui/dashboard.cpp
        src/app/settings.cpp
        src/app/logger.cpp
        src/app/data_provider.cpp
        src/app/data_service.cpp
        ${IMGUI_SOURCES}
    )

    add_custom_target(run
        COMMAND SystemMonitor
        DEPENDS SystemMonitor
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )

    target_include_directories(SystemMonitor PRIVATE
        src
        external/imgui
        external/imgui/backends
        external/implot
        external/glfw/include
    )

    if(APPLE)
        target_link_libraries(SystemMonitor
            glfw
            "-framework OpenGL"
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
        )
    else()
        target_link_libraries(SystemMonitor glfw OpenGL::GL)
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(SystemMonitor PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_library(monitor_core STATIC
        src/app/settings.cpp
        src/app/data_provider.cpp
        src/app/data_service.cpp
        src/app/logger.cpp
    )
    target_include_directories(monitor_core PUBLIC src)

    add_executable(test_settings tests/test_settings.cpp)
    target_link_libraries(test_settings PRIVATE monitor_core)
    add_test(NAME test_settings COMMAND test_settings)

    add_executable(test_data_provider tests/test_data_provider.cpp)
    target_link_libraries(test_data_provider PRIVATE monitor_core)
    add_test(NAME test_data_provider COMMAND test_data_provider)

    add_executable(test_data_service tests/test_data_service.cpp)
    target_link_libraries(test_data_service PRIVATE monitor_core)
    add_test(NAME test_data_service COMMAND test_data_service)

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(monitor_core PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
