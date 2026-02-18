cmake_minimum_required(VERSION 3.20)
project(MesozoicGenesis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== VULKAN SDK ====================
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK found: ${Vulkan_LIBRARY}")
    add_compile_definitions(VULKAN_SDK_AVAILABLE=1)
else()
    message(STATUS "Vulkan SDK not found - building without GPU rendering")
    add_compile_definitions(VULKAN_SDK_AVAILABLE=0)
endif()

# ==================== GLFW (optional) ====================
option(BUILD_HEADLESS "Build without graphics dependencies" OFF)

if(NOT BUILD_HEADLESS)
    # Try to find GLFW via find_package first (system install)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        message(STATUS "GLFW found via find_package")
        set(GLFW_AVAILABLE TRUE)
    else()
        # Try FetchContent if git is available
        find_program(GIT_PROGRAM git)
        if(GIT_PROGRAM)
            message(STATUS "Fetching GLFW via FetchContent...")
            include(FetchContent)
            FetchContent_Declare(
                glfw
                GIT_REPOSITORY https://github.com/glfw/glfw.git
                GIT_TAG        3.4
                GIT_SHALLOW    TRUE
            )
            set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(glfw)
            set(GLFW_AVAILABLE TRUE)
        else()
            message(STATUS "GLFW not found and git not available - building headless (no windowing)")
            set(GLFW_AVAILABLE FALSE)
        endif()
    endif()
else()
    message(STATUS "Building HEADLESS (explicitly requested) - skipping GLFW")
    set(GLFW_AVAILABLE FALSE)
endif()

# ==================== GLM ====================
include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ==================== TESTS ====================
set(TEST_SOURCES Tests/CoreTests.cpp)

add_executable(MesozoicTests ${TEST_SOURCES})
target_link_libraries(MesozoicTests PRIVATE glm::glm)
target_include_directories(MesozoicTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ==================== GAME ====================
set(GAME_SOURCES Game/Main.cpp Graphics/TerrainSystem.cpp Graphics/UI/UISystem.cpp)

add_executable(MesozoicGenesis ${GAME_SOURCES})
target_link_libraries(MesozoicGenesis PRIVATE glm::glm)
target_include_directories(MesozoicGenesis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link optional dependencies
if(GLFW_AVAILABLE)
    target_link_libraries(MesozoicGenesis PRIVATE glfw)
    target_compile_definitions(MesozoicGenesis PRIVATE GLFW_LINKED=1)
    if(DEFINED glfw_SOURCE_DIR)
        target_include_directories(MesozoicGenesis PRIVATE ${glfw_SOURCE_DIR}/include)
        target_include_directories(MesozoicTests PRIVATE ${glfw_SOURCE_DIR}/include)
    endif()
endif()

if(Vulkan_FOUND)
    target_link_libraries(MesozoicGenesis PRIVATE Vulkan::Vulkan)
    target_include_directories(MesozoicGenesis PRIVATE ${Vulkan_INCLUDE_DIRS})
    
    target_link_libraries(MesozoicTests PRIVATE Vulkan::Vulkan)
    target_include_directories(MesozoicTests PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# ==================== COMPILER OPTIONS ====================
if(MSVC)
    target_compile_options(MesozoicTests PRIVATE /W4 /WX-)
    target_compile_options(MesozoicGenesis PRIVATE /W4 /WX-)
else()
    target_compile_options(MesozoicTests PRIVATE -Wall)
    target_compile_options(MesozoicGenesis PRIVATE -Wall)
endif()

# ==================== SHADER COMPILATION ====================
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")
    set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/Shaders")
    set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
    file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})
    
    add_custom_command(
        OUTPUT ${SHADER_BINARY_DIR}/vert.spv
        COMMAND ${GLSLC} ${SHADER_SOURCE_DIR}/shader.vert -o ${SHADER_BINARY_DIR}/vert.spv
        DEPENDS ${SHADER_SOURCE_DIR}/shader.vert
        COMMENT "Compiling shader.vert"
    )
    add_custom_command(
        OUTPUT ${SHADER_BINARY_DIR}/frag.spv
        COMMAND ${GLSLC} ${SHADER_SOURCE_DIR}/shader.frag -o ${SHADER_BINARY_DIR}/frag.spv
        DEPENDS ${SHADER_SOURCE_DIR}/shader.frag
        COMMENT "Compiling shader.frag"
    )
    add_custom_command(
        OUTPUT ${SHADER_BINARY_DIR}/ui.vert.spv
        COMMAND ${GLSLC} ${SHADER_SOURCE_DIR}/ui.vert -o ${SHADER_BINARY_DIR}/ui.vert.spv
        DEPENDS ${SHADER_SOURCE_DIR}/ui.vert
        COMMENT "Compiling ui.vert"
    )
    add_custom_command(
        OUTPUT ${SHADER_BINARY_DIR}/ui.frag.spv
        COMMAND ${GLSLC} ${SHADER_SOURCE_DIR}/ui.frag -o ${SHADER_BINARY_DIR}/ui.frag.spv
        DEPENDS ${SHADER_SOURCE_DIR}/ui.frag
        COMMENT "Compiling ui.frag"
    )
    
    add_custom_target(CompileShaders ALL DEPENDS
        ${SHADER_BINARY_DIR}/vert.spv
        ${SHADER_BINARY_DIR}/frag.spv
        ${SHADER_BINARY_DIR}/ui.vert.spv
        ${SHADER_BINARY_DIR}/ui.frag.spv
    )
    
    # Copy shaders to executable directory for Debug/Release
    add_custom_command(TARGET CompileShaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_BINARY_DIR}/vert.spv $<TARGET_FILE_DIR:MesozoicGenesis>/vert.spv
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_BINARY_DIR}/frag.spv $<TARGET_FILE_DIR:MesozoicGenesis>/frag.spv
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_BINARY_DIR}/ui.vert.spv $<TARGET_FILE_DIR:MesozoicGenesis>/ui.vert.spv
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_BINARY_DIR}/ui.frag.spv $<TARGET_FILE_DIR:MesozoicGenesis>/ui.frag.spv
    )
    
    add_dependencies(MesozoicGenesis CompileShaders)
else()
    message(WARNING "glslc not found! Shaders will not be compiled. Graphics pipeline will fail.")
endif()
