add_executable(CameraService Src/main.cpp)
add_executable(CameraTests Test/Test.cpp)

target_link_libraries(CameraTests PRIVATE GTest::gtest_main)

gtest_discover_tests(CameraTests)