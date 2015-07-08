set(ACCEL "bvh" CACHE STRING "Acceleration structure type (bvh, ...)")
set(VECTORIZE "" CACHE STRING "Hand coded vectorization length (1, 4, 8, 16)")

if(ACCEL STREQUAL "bvh")
    set(ACCEL_SRCS accel/bvh.impala)
else()
    message(FATAL_ERROR "Invalid acceleration structure type specified")
endif()

set(COMMON_SRCS
    common/debug.impala
    common/float.impala
    common/intersection.impala
    common/range.impala
    common/stack.impala
    common/vector.impala
    common/traversal.impala)

if(VECTORIZE STREQUAL "" OR VECTORIZE STREQUAL "1")
    set(VECTORIZE_SRCS vectorize/scalar.impala)
elseif(VECTORIZE STREQUAL "4")
    set(VECTORIZE_SRCS vectorize/vectorize4.impala)
elseif(VECTORIZE STREQUAL "8")
    set(VECTORIZE_SRCS vectorize/vectorize8.impala)
elseif(VECTORIZE STREQUAL "16")
    set(VECTORIZE_SRCS vectorize/vectorize16.impala)
else()
    message(FATAL_ERROR "Invalid vector size specified for hand-coded mapping")
endif()

if(BACKEND STREQUAL "cpu")
    set(MAPPING_SRCS mappings/mapping_packet.impala)
elseif(BACKEND STREQUAL "avx")
    set(MAPPING_SRCS mappings/mapping_packet_auto.impala)
elseif(BACKEND STREQUAL "nvvm")
    set(MAPPING_SRCS mappings/mapping_gpu.impala)
else()
    message(FATAL_ERROR "Backend not supported")
endif()

set(IMPALA_SRCS ${ACCEL_SRCS} ${MAPPING_SRCS} ${VECTORIZE_SRCS} ${COMMON_SRCS})

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/frontend/interface.h
                   COMMAND impala -emit-c-interface ${IMPALA_SRCS}
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o interface
                   DEPENDS ${IMPALA_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS})

set_source_files_properties(interface.h PROPERTIES GENERATED TRUE)

add_library(traversal ${IMPALA_OBJS})
target_link_libraries(traversal ${IMPALA_LIBS})

