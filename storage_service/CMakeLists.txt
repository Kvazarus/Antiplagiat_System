cmake_minimum_required(VERSION 3.14)
project(storage_service)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(BUILD_SHARED_LIBS OFF)

include(FetchContent)
FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.10.0
)
set(CPR_USE_SYSTEM_CURL OFF)
set(BUILD_CPR_TESTS OFF)
FetchContent_MakeAvailable(cpr)

include_directories(include)
add_executable(storage_service main.cpp)

target_link_libraries(storage_service
        PRIVATE
        ${PQXX_LIBRARIES}
        cpr::cpr
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
)