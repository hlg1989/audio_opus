cmake_minimum_required(VERSION 3.8)
project(audio_opus)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp audio_pcm_grab.cpp audiofifo.cpp audio_opus_encode.cpp)
add_executable(audio_opus ${SOURCE_FILES})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
message( "CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS system filesystem thread regex date_time chrono REQUIRED)
if(Boost_FOUND)
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
endif()

find_package(CppREST REQUIRED)
if(cpprest_FOUND)
    target_link_libraries(${PROJECT_NAME} ${cpprest_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} -lssl -lcrypto)
endif()

find_package(FFmpeg REQUIRED)
if(FFMPEG_FOUND)
    target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})
endif()

find_package (Threads REQUIRED)
if(Threads_FOUND)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
endif()

target_link_libraries(${PROJECT_NAME} -lasound -ldl -lm)
