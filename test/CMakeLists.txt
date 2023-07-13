cmake_minimum_required(VERSION 3.25 )
cmake_policy(VERSION 3.25)
set(CMAKE_CXX_STANDARD 17)

project ( nasdaq_tests )

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_executable( nasdaq_tests 
    itch.cpp
    ouch.cpp
    soupbintcp.cpp
    soupbinserver.cpp
    ../src/soup_bin_timer.cpp
    ../src/soup_bin_connection.cpp
)

target_include_directories(nasdaq_tests PRIVATE 
    ../include
)

target_link_libraries(nasdaq_tests 
    GTest::gtest
    GTest::gtest_main
)

