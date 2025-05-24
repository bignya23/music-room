cmake_minimum_required(VERSION 3.15)
project(MyBoostAsioApp)
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

# ——— Prepare our target ———
add_executable(MyBoostAsioApp
  music-room.cpp
  src/client.cpp
  src/server.cpp
  src/vote.cpp
  src/music.cpp
  
)

# ——— Include directories ———
target_include_directories(MyBoostAsioApp
  PUBLIC
    ${Boost_INCLUDE_DIRS}          # <boost/...>
    ${CMAKE_SOURCE_DIR}/src/headers  # for .hpp files
)

target_include_directories(MyBoostAsioApp
  PUBLIC
    ${BOOST_ROOT}                  # for Beast, Asio, etc.
)

# ——— Link libraries ———
target_link_libraries(MyBoostAsioApp
  PRIVATE
    ${Boost_LIBRARIES}
)
