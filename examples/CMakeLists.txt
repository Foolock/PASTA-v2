set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/examples)

set(PASTA_EXAMPLES
  simple
  manual
  incre_semaphore 
  incre_cudaflow_partition 
  modify_edges
  modify_edges_with_pasta 
  modify_edges_with_pasta_round_robin 
  modify_edges_with_pasta_full 
  modify_edges_with_pasta_seq
  modify_edges_with_cudaflow 
  modify_edges_with_cudaflow_update 
  modify_edges_with_cudaflow_update_incre 
  modify_nodes_with_cudaflow_update_incre
  # modify_nodes_with_cudaflow_update_incre_stable
  modify_nodes_with_cudaflow_update_incre_rebuild_stream_stable
  modify_nodes_with_semaphore
  modify_nodes_with_semaphore_stable
  mis
  cudaflow_partition
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
