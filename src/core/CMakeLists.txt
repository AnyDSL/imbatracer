set(core_SRC
assert.h
macros.h
refcounted.h
util.h
util.cpp
)


add_library(core ${core_SRC})
target_link_libraries(core ${PNG_LIBRARIES} ${SDL2_LIBRARIES})

