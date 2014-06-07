set(CUDA_DIR "/opt/cuda6.0" CACHE STRING "The directory containing the CUDA toolkit")
set(RUNTIME_DIR "/home/r/Dokumente/Unisachen/cg-hiwi/thorin/runtime" CACHE STRING "The directory containing the thorin runtime")

set(THORIN_INCLUDE_DIRS ${CUDA_DIR}/include ${CUDA_DIR}/nvvm/include ${CUDA_DIR}/nvvm/libnvvm-samples/common/include ${RUNTIME_DIR}/cuda)
set(THORIN_LIBRARY_DIRS ${CUDA_DIR}/lib64 ${CUDA_DIR}/nvvm/lib64)
set(THORIN_LIBRARIES cuda nvvm)

macro(IMPALA_WRAP outfiles)
	# add the runtime
	set(${outfiles} ${${outfiles}} ${RUNTIME_DIR}/cuda/cu_runtime.cpp)
	SET_SOURCE_FILES_PROPERTIES(
		${RUNTIME_DIR}/cuda/cu_runtime.cpp
		PROPERTIES
		COMPILE_FLAGS "'-DLIBDEVICE_DIR=\"${CUDA_DIR}/nvvm/libdevice/\"'")
	# get the options right
	set(CLANG_OPTS ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE})
	separate_arguments(CLANG_OPTS)
	# go over input files
	foreach (it ${ARGN})
		# get name of files
		get_filename_component(infile ${it} ABSOLUTE)
		get_filename_component(basefile ${it} NAME_WE)
		set(llfile ${basefile}.ll)
		get_filename_component(llfile ${llfile} ABSOLUTE)
		set(objfile ${CMAKE_CURRENT_BINARY_DIR}/${basefile}.o)
		# tell cmake what to do
		add_custom_command(OUTPUT ${llfile}
			COMMAND impala
			ARGS ${infile} -emit-llvm
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${infile} VERBATIM)
		add_custom_command(OUTPUT ${objfile}
			COMMAND clang++
			ARGS ${CLANG_OPTS} -c -o ${objfile} ${llfile}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${llfile} VERBATIM)
		SET_SOURCE_FILES_PROPERTIES(
			${outfile}
			PROPERTIES
			EXTERNAL_OBJECT true
			GENERATED true)
		set(${outfiles} ${${outfiles}} ${objfile})
	endforeach()
endmacro()
