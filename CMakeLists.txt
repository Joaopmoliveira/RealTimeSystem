cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0091 NEW)
project(Curan)

#Language definitions used throught the entire project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif(WIN32)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")
set(CMAKE_INSTALL_LIBDIR "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#Find all required third parties (this should be moved elsewhere)
find_package(asio CONFIG REQUIRED)

# Watchdog 
add_executable(watchdog watchdog.cpp)
target_link_libraries(watchdog PUBLIC asio)

# Simulation of the sensors 
add_executable(sensorsimulation sensors.cpp)
target_link_libraries(sensorsimulation PUBLIC asio)

# Simulation of student code
add_executable(besteffortapp students_code.cpp)
target_link_libraries(besteffortapp PUBLIC asio)

# Watchdog Test Jetson
add_executable(watchdogTestJetson watchdog_test_jetson.cpp)
target_link_libraries(watchdogTestJetson PUBLIC asio)

# GPIO Jetson
add_executable(gpio_jetson gpio_nvidea.cpp)
target_link_libraries(gpio_jetson PUBLIC asio)