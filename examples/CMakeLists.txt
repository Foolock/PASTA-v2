set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/examples)

set(PASTA_EXAMPLES
  simple
  exp
)

foreach(example IN LISTS PASTA_EXAMPLES)
  add_executable(${example} ${example}.cpp)
endforeach()

find_package(Threads REQUIRED)
foreach(example IN LISTS PASTA_EXAMPLES)
  target_include_directories(${example}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/pasta)

  # link external libaries
  target_link_libraries(${example}
    PRIVATE Threads::Threads
    PUBLIC pasta
  )
endforeach()
