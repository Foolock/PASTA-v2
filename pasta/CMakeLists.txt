add_library(pasta pasta.cpp)

# include taskflow
target_include_directories(pasta
  PUBLIC ${PASTA_3RD_PARTY_DIR}/taskflow)

# find the pthreads library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(pasta PRIVATE Threads::Threads)
