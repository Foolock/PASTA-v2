enable_testing()

include(${PASTA_3RD_PARTY_DIR}/doctest/doctest.cmake)

list(APPEND PASTA_UNITTESTS
check_cudaflow_partition
check_incre_cudaflow_partition
)

string(FIND '${CMAKE_CXX_FLAGS}' "-fsanitize" sanitize)

foreach(unittest IN LISTS PASTA_UNITTESTS)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/unittests)
  add_executable(${unittest} ${unittest}.cpp)
  target_include_directories(${unittest}
    PUBLIC 
    ${PASTA_3RD_PARTY_DIR}/doctest
    ${PASTA_3RD_PARTY_DIR}/taskflow
    ${PROJECT_SOURCE_DIR}/pasta
  )
  target_link_libraries(${unittest}
    PUBLIC
    pasta 
  )
  doctest_discover_tests(${unittest})
endforeach()
