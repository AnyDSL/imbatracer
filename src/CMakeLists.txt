# Get the Impala source files names
add_subdirectory(impala)
get_directory_property(IMPALA_SRCS DIRECTORY impala DEFINITION IMPALA_SRCS)

set(IMPALA_SRCS_PREFIX "")
foreach(FILE IN ITEMS ${IMPALA_SRCS})
    set(NEW_FILE "impala/${FILE}")
    list(APPEND IMPALA_SRCS_PREFIX ${NEW_FILE})
endforeach(FILE IN ${IMPALA_SRCS})

thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS_PREFIX})

# Generate the C interface
add_custom_target(impala-interface DEPENDS ${IMPALA_SRCS_PREFIX}
                  COMMAND impala -emit-c-interface ${IMPALA_SRCS} ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala -o impala_interface
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/impala)
set_source_files_properties(impala/impala_interface.h PROPERTIES GENERATED TRUE)

# Generate the core library
add_library(imba_core
            ${IMPALA_OBJS}
            scene/image.hpp
            scene/instance.hpp
            scene/light.hpp
            scene/material.hpp
            scene/object.hpp
            scene/proxy.hpp
            scene/render.cpp
            scene/render.hpp
            scene/scene.cpp
            scene/scene.hpp
            scene/triangle_mesh.hpp
            common/logger.hpp
            common/matrix.hpp
            common/memory.hpp
            common/options.hpp
            common/path.hpp
            common/vector.hpp)

add_library(imba_devices
            devices/device.hpp
            devices/png_device.cpp
            devices/png_device.hpp
            devices/sdl_device.cpp
            devices/sdl_device.hpp)

add_library(imba_loaders
            loaders/loader.hpp
            loaders/image_loader.hpp
            loaders/scene_loader.hpp
            loaders/png_loader.cpp
            loaders/png_loader.hpp
            loaders/obj_loader.cpp
            loaders/obj_loader.hpp)

add_dependencies(imba_core impala-interface)

target_link_libraries(imba_core  ${IMPALA_LIBS})
target_link_libraries(imba_devices ${PNG_LIBRARIES} ${SDL_LIBRARY})
target_link_libraries(imba_loaders ${PNG_LIBRARIES})

# Generate the main executable
add_executable(imbatracer
               main.cpp)

target_link_libraries(imbatracer imba_devices)
target_link_libraries(imbatracer imba_loaders)
target_link_libraries(imbatracer imba_core)

# Set compiler options
function(ADD_CXX_FLAG target opt)
    get_target_property(CXX_FLAGS ${target} COMPILE_FLAGS)
    if(CXX_FLAGS STREQUAL "CXX_FLAGS-NOTFOUND")
      set(CXX_FLAGS "")
    endif()
    set(CXX_FLAGS "${CXX_FLAGS} ${opt}")
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${CXX_FLAGS})
endfunction(ADD_CXX_FLAG)

add_cxx_flag(imba_core    "-std=c++11")
add_cxx_flag(imba_loaders "-std=c++11")
add_cxx_flag(imba_devices "-std=c++11")
add_cxx_flag(imbatracer   "-std=c++11")

