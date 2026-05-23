# MyEngine/CMakeLists.txt

cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE PATH "vcpkg toolchain" FORCE)
    elseif(EXISTS "C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE PATH "vcpkg toolchain" FORCE)
    else()
        message(FATAL_ERROR
            "CMAKE_TOOLCHAIN_FILE 縺梧悴險ｭ螳壹〒縺吶Ｗcpkg 繧剃ｽｿ縺・・ｽ・ｽ縺ｯ谺｡縺ｮ縺・・ｽ・ｽ繧後°縺悟ｿ・・ｽ・ｽ縺ｧ縺・\n"
            "  窶｢ cmake --preset windows-vcpkg\n"
            "  窶｢ cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake\n"
            "  窶｢ 迺ｰ蠅・・ｽ・ｽ謨ｰ VCPKG_ROOT 繧・vcpkg 縺ｮ繧ｯ繝ｭ繝ｼ繝ｳ蜈医↓險ｭ螳壹＠縺ｦ縺九ｉ蜀榊ｮ溯｡圭n")
    endif()
endif()

project(MyEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(flecs REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(poly2tri CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_INT_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SHADER_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MyEngine/shaders")
file(MAKE_DIRECTORY "${SHADER_INT_DIR}")

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows")
endif()
find_program(GLSLANG_VALIDATOR NAMES glslangValidator glslangValidator.exe
    HINTS
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/glslang"
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin"
    REQUIRED
    DOC "SPIR-V compiler from glslang[tools] (vcpkg)"
)

# -----------------------------------------------------------------------------
# Shader compilation (auto-discovered).
#   Every *.vert/*.frag/*.comp under shaders/ is compiled to
#   ${SHADER_INT_DIR}/<name>_<stage>.spv (e.g. grass_instanced.vert ->
#   grass_instanced_vert.spv), preserving the existing naming the C++ loaders
#   expect. Each shader DEPENDS on its source AND on every shared header
#   (shared/*.h), so editing shared/types.h recompiles all shaders that include
#   it. CONFIGURE_DEPENDS re-globs when files are added/removed, so adding a
#   shader needs no CMake edit. No hand-maintained OUTPUT/COMMAND/DEPENDS lists.
# -----------------------------------------------------------------------------
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SRC_DIR}/*.vert"
    "${SHADER_SRC_DIR}/*.frag"
    "${SHADER_SRC_DIR}/*.comp")
file(GLOB SHADER_SHARED_HEADERS CONFIGURE_DEPENDS
    "${SHADER_INC_DIR}/shared/*.h"
    "${SHADER_INC_DIR}/shared/*.glsl")

set(SPIRV_OUTPUTS "")
foreach(_shader ${SHADER_SOURCES})
    get_filename_component(_name ${_shader} NAME)        # grass_instanced.vert
    string(REPLACE "." "_" _stem ${_name})               # grass_instanced_vert
    set(_spv "${SHADER_INT_DIR}/${_stem}.spv")
    add_custom_command(
        OUTPUT  "${_spv}"
        COMMAND "${GLSLANG_VALIDATOR}" -V "-I${SHADER_INC_DIR}" "${_shader}" -o "${_spv}"
        DEPENDS "${_shader}" ${SHADER_SHARED_HEADERS}
        COMMENT "SPIR-V ${_name}"
        VERBATIM)
    list(APPEND SPIRV_OUTPUTS "${_spv}")
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})

add_executable(${PROJECT_NAME}
    src/main.cpp

    src/app/engine_app.cpp

    src/core/attack_registry.cpp
    src/core/chest_preset.cpp
    src/core/enemy_hitbox_util.cpp
    src/core/equipment_util.cpp
    src/core/event_bus.cpp
    src/core/grave_fx.cpp
    src/core/grip.cpp
    src/core/input_binding.cpp
    src/core/potion.cpp
    src/core/settings_io.cpp
    
    src/loop/choice_overlay_layer.cpp
    src/loop/default_layer_factory.cpp
    src/loop/game_loop_orchestrator.cpp
    src/loop/game_over_layer.cpp
    src/loop/gameplay_layer.cpp
    src/loop/graphics_settings_layer.cpp
    src/loop/key_config_layer.cpp
    src/loop/layer_stack.cpp
    src/loop/menu_layer_base.cpp
    src/loop/mode_select_layer.cpp
    src/loop/settings_layer.cpp
    src/loop/title_layer.cpp

    src/renderer/animation.cpp
    src/renderer/animator.cpp
    src/renderer/asset_registry.cpp
    src/renderer/bindless_texture_registry.cpp
    src/renderer/debug_line_pass.cpp
    src/renderer/frame_sync.cpp
    src/renderer/frame_uniforms.cpp
    src/renderer/hud_pass.cpp
    src/renderer/hud_pipeline.cpp
    src/renderer/imgui_layer.cpp
    src/renderer/particle_pass.cpp
    src/renderer/vulkan_context.cpp
    src/renderer/resource_factory.cpp
    src/renderer/vulkan_renderer.cpp
    src/renderer/swapchain.cpp
    src/renderer/texture.cpp
    src/renderer/main_pass.cpp
    src/renderer/mesh.cpp
    src/renderer/model.cpp
    src/renderer/model_loader.cpp
    src/renderer/model_scale_registry.cpp
    src/renderer/pass_chain.cpp
    src/renderer/post_pass.cpp
    src/renderer/bloom_pass.cpp
    src/renderer/reflection_pass.cpp
    src/renderer/reflection_target.cpp
    src/renderer/render_target.cpp
    src/renderer/terrain_mesh.cpp
    src/renderer/shader_util.cpp
    src/renderer/shadow_pass.cpp
    src/renderer/skeleton.cpp
    src/renderer/skin_buffer_pool.cpp
    src/renderer/instance_buffer_pool.cpp
    src/renderer/material_registry.cpp
    src/renderer/water_mesh.cpp
    src/renderer/water_pass.cpp
    src/renderer/water_pipeline.cpp

    src/scene/scene_data.cpp
    src/scene/scene_renderer.cpp

    src/systems/anim_state_system.cpp
    src/systems/audio_event_system.cpp
    src/systems/camera_system.cpp
    src/systems/chest_system.cpp
    src/systems/combat_system.cpp
    src/systems/enemy_system.cpp
    src/systems/gate_system.cpp
    src/systems/hud_system.cpp
    src/systems/input_system.cpp
    src/systems/item_physics_system.cpp
    src/systems/item_pickup_system.cpp
    src/systems/keyboard_mouse_device.cpp
    src/systems/movement_system.cpp
    src/systems/moving_platform_system.cpp
    src/systems/particle_system.cpp
    src/systems/physics_system.cpp
    src/systems/render_debug_system.cpp
    src/systems/simulation_system.cpp
    src/systems/skeletal_anim_system.cpp
    src/systems/spin_animation_system.cpp
    src/systems/sound_manager.cpp
    src/systems/spawn_system.cpp
    src/systems/spirit_system.cpp
    src/systems/demonVillageBgmGenerator.cpp
    src/world/stage_registry.cpp
    src/world/world_builder.cpp
    src/world/world_terrain.cpp
)
add_dependencies(${PROJECT_NAME} compile_shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SHADER_INT_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${PROJECT_NAME}>/vulkan-1.dll"
    COMMENT "Copy SPIR-V shaders; remove duplicate Vulkan loader next to exe"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    glm::glm
    flecs::flecs
    tinyobjloader::tinyobjloader
    imgui::imgui
    assimp::assimp
    poly2tri::poly2tri
    GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${PROJECT_NAME} PUBLIC
    include/MyEngine
)
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${MINIAUDIO_INCLUDE_DIR})
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /EHsc)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copy assets to output directory"
)
