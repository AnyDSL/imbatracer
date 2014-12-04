
set(io_SRC
image.h
image.cpp
sdlgui.h
sdlgui.cpp
sdlrenderer.h
sdlrenderer.cpp
)


add_library(io ${io_SRC})
target_link_libraries(io core ${PNG_LIBRARIES} ${SDL2_LIBRARIES})

