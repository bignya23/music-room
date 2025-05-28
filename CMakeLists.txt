cmake_minimum_required(VERSION 3.15)
project(MyBoostAsioApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ——— Find Boost ———
set(BOOST_ROOT "C:/Libraries/boost_1_88_0")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS system thread)
if (NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found in ${BOOST_ROOT}")
endif()

# ——— Add PortAudio ———
set(PORTAUDIO_INCLUDE_DIR "C:/vcpkg-master/packages/portaudio_x64-windows/include")
set(PORTAUDIO_LIBRARY "C:/vcpkg-master/packages/portaudio_x64-windows/lib/portaudio.lib")

# ——— Prepare target ———
add_executable(music_room
  music-room.cpp
  src/client.cpp
  src/server.cpp
  src/vote.cpp
  src/music.cpp
  src/rooms.cpp
)

# ——— Include directories ———
target_include_directories(music_room PUBLIC
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/headers
  ${BOOST_ROOT}
  ${PORTAUDIO_INCLUDE_DIR}
)

# ——— Link libraries ———
target_link_libraries(music_room
  PRIVATE
    ${Boost_LIBRARIES}
    ${PORTAUDIO_LIBRARY}
)


add_custom_command(TARGET music_room POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/vcpkg-master/installed/x64-windows/bin/portaudio.dll"
        $<TARGET_FILE_DIR:music_room>
)
