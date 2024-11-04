add_executable(cpplsp)

target_sources(cpplsp PRIVATE main.cpp)

target_compile_features(cpplsp PRIVATE cxx_std_20)

add_library(Args)

target_sources(
  Args
  PUBLIC args.cpp
  PUBLIC FILE_SET HEADERS FILES args.h)

target_include_directories(Args PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(Args PRIVATE cxx_std_20)

target_link_libraries(cpplsp PRIVATE Args)
