cmake_minimum_required (VERSION 3.8)

if ($ENV{GITHUB_RUN_NUMBER})
    project (tcp-echo-server-io-uring VERSION 0.0.$ENV{GITHUB_RUN_NUMBER})
else()
    project (tcp-echo-server-io-uring VERSION 0.0.1)
endif()

find_package(Doxygen)

option(WITH_LIBURING "with liburing" ON)

set(PROJECT_SOURCES
    tcp-echo-server-io-uring.cpp
    echo-server.h
    echo-server.cpp
)

add_executable (tcp-echo-server-io-uring ${PROJECT_SOURCES})

if(WITH_LIBURING)
   include(FetchContent)

   FetchContent_Declare(
       liburing_cmake
       GIT_REPOSITORY https://github.com/sharpeye/liburing-cmake.git
       GIT_TAG main
   )

   FetchContent_MakeAvailable(liburing_cmake)

endif()

target_link_libraries(tcp-echo-server-io-uring PRIVATE liburing)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tcp-echo-server-io-uring PROPERTY CXX_STANDARD 20)
endif()

install(TARGETS tcp-echo-server-io-uring RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT example@example.com)
include(CPack)

