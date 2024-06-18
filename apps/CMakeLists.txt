add_executable(demo0 demo0.cpp)
target_link_libraries(demo0 PUBLIC apricot)

add_executable(demo1 demo1.cpp)
target_link_libraries(demo1 PUBLIC apricot)

add_executable(demo2 demo2.cpp)
target_link_libraries(demo2 PUBLIC apricot)


if (${SANITIZE_Q_APPS})
  # address
  add_executable(demo0_asan demo0.cpp)
  target_link_libraries(demo0_asan PUBLIC apricot)
  target_compile_options(demo0_asan PRIVATE -fsanitize=address)
  target_link_options(demo0_asan PRIVATE -fsanitize=address)

  add_executable(demo1_asan demo1.cpp)
  target_link_libraries(demo1_asan PUBLIC apricot)
  target_compile_options(demo1_asan PRIVATE -fsanitize=address)
  target_link_options(demo1_asan PRIVATE -fsanitize=address)

  add_executable(demo2_asan demo2.cpp)
  target_link_libraries(demo2_asan PUBLIC apricot)
  target_compile_options(demo2_asan PRIVATE -fsanitize=address)
  target_link_options(demo2_asan PRIVATE -fsanitize=address)

  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin|Linux")
    # thread
    add_executable(demo0_tsan demo0.cpp)
    target_link_libraries(demo0_tsan PUBLIC apricot)
    target_compile_options(demo0_tsan PRIVATE -fsanitize=address)
    target_link_options(demo0_tsan PRIVATE -fsanitize=address)

    add_executable(demo1_tsan demo1.cpp)
    target_link_libraries(demo1_tsan PUBLIC apricot)
    target_compile_options(demo1_tsan PRIVATE -fsanitize=thread)
    target_link_options(demo1_tsan PRIVATE -fsanitize=thread)

    add_executable(demo2_tsan demo2.cpp)
    target_link_libraries(demo2_tsan PUBLIC apricot)
    target_compile_options(demo2_tsan PRIVATE -fsanitize=thread)
    target_link_options(demo2_tsan PRIVATE -fsanitize=thread)

    # undefined behavior
    add_executable(demo0_usan demo0.cpp)
    target_link_libraries(demo0_usan PUBLIC apricot)
    target_compile_options(demo0_usan PRIVATE -fsanitize=undefined)
    target_link_options(demo0_usan PRIVATE -fsanitize=undefined)

    add_executable(demo1_usan demo1.cpp)
    target_link_libraries(demo1_usan PUBLIC apricot)
    target_compile_options(demo1_usan PRIVATE -fsanitize=undefined)
    target_link_options(demo1_usan PRIVATE -fsanitize=undefined)

    add_executable(demo2_usan demo2.cpp)
    target_link_libraries(demo2_usan PUBLIC apricot)
    target_compile_options(demo2_usan PRIVATE -fsanitize=undefined)
    target_link_options(demo2_usan PRIVATE -fsanitize=undefined)
  endif ()

endif ()
