cmake_minimum_required(VERSION 3.10)
project(analysis_service)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
find_package(Threads REQUIRED)

include_directories(include)

add_executable(analysis_service main.cpp)

target_link_libraries(analysis_service
        ${PQXX_LIBRARIES}
        Threads::Threads
)