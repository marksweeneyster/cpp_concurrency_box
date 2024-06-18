FetchContent_Declare(googletest
    GIT_REPOSITORY GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(apricot_test apricot_test.cpp)

target_link_libraries(apricot_test
    PUBLIC
    GTest::gtest_main
    apricot
)

gtest_discover_tests(apricot_test)
