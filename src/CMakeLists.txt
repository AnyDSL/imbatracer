# Get the Impala source files names
add_subdirectory(impala)
get_directory_property(IMPALA_SRCS           DIRECTORY impala DEFINITION IMPALA_SRCS)
get_directory_property(IMPALA_BENCH_SRCS     DIRECTORY impala DEFINITION IMPALA_BENCH_SRCS)
get_directory_property(IMPALA_INTERFACE_SRCS DIRECTORY impala DEFINITION IMPALA_INTERFACE_SRCS)

# Add a directory prefix to the impala files
function(ADD_PREFIX list prefix_list prefix)
    set(new_list "")
    foreach(ITEM IN ITEMS ${list})
        set(NEW_ITEM "${prefix}${ITEM}")
        list(APPEND new_list ${NEW_ITEM})
    endforeach(ITEM IN ITEMS ${list})
    set(${prefix_list} "${new_list}" PARENT_SCOPE)
endfunction(ADD_PREFIX)

add_prefix("${IMPALA_SRCS}"           IMPALA_SRCS_PREFIX           "impala/")
add_prefix("${IMPALA_BENCH_SRCS}"     IMPALA_BENCH_SRCS_PREFIX     "impala/")
add_prefix("${IMPALA_INTERFACE_SRCS}" IMPALA_INTERFACE_SRCS_PREFIX "impala/")

# Compatibility with old GLIBC
set(CMAKE_SHARED_LINKER_FLAGS "-static-libstdc++ -Wl,--wrap=memcpy -Wl,--wrap=clock_gettime -lrt")
set(COMPAT_SRCS memcpy.c clock_gettime.c)

# Do not include the benchmarks
thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS_PREFIX} ${IMPALA_INTERFACE_SRCS_PREFIX})

# Generate the C interface
add_custom_target(impala_interface DEPENDS ${IMPALA_SRCS_PREFIX} ${IMPALA_BENCH_SRCS_PREFIX} ${IMPALA_INTERFACE_SRCS_PREFIX}
                  COMMAND impala -emit-c-interface ${IMPALA_SRCS} ${IMPALA_BENCH_SRCS} ${IMPALA_INTERFACE_SRCS}
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BENCH_BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o impala_interface
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/impala)
set_source_files_properties(impala/impala_interface.h PROPERTIES GENERATED TRUE)

# Generate the core library
add_library(imba_core
            ${COMPAT_SRCS}
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
            ${COMPAT_SRCS}
            devices/device.hpp
            devices/png_device.cpp
            devices/png_device.hpp
            devices/sdl_device.cpp
            devices/sdl_device.hpp)

add_library(imba_loaders
            ${COMPAT_SRCS}
            loaders/loader.hpp
            loaders/image_loader.hpp
            loaders/scene_loader.hpp
            loaders/png_loader.cpp
            loaders/png_loader.hpp
            loaders/tga_loader.cpp
            loaders/tga_loader.hpp
            loaders/obj_loader.cpp
            loaders/obj_loader.hpp)

add_dependencies(imba_core impala_interface)

target_link_libraries(imba_core  ${IMPALA_LIBS})
target_link_libraries(imba_devices ${PNG_LIBRARIES} ${SDL_LIBRARY})
target_link_libraries(imba_loaders ${PNG_LIBRARIES})

# Generate the main executable
add_executable(imbatracer
               main.cpp)

target_link_libraries(imbatracer imba_devices)
target_link_libraries(imbatracer imba_loaders)
target_link_libraries(imbatracer imba_core)

