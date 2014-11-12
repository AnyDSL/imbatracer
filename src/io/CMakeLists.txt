
set(io_SRC
image.h
image.cpp
sdlgui.h
sdlgui.cpp
sdlguithread.h
sdlguithread.cpp
sdlrenderer.h
sdlrenderer.cpp
sdlbuffergui.h
sdlbuffergui.cpp
)


add_library(io ${io_SRC})
target_link_libraries(io core ${PNG_LIBRARIES} ${SDL2_LIBRARIES})

