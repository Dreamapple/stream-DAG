cmake_minimum_required(VERSION 3.8.2)
project(StreamDagProject)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3 -std=c++17")

# 添加头文件依赖目录
include_directories(include worker)

# 添加源代码文件
add_executable(benchmark_app benchmark.cc)

# 查找并添加第三方库
find_package(gflags REQUIRED)
find_package(Protobuf REQUIRED)
find_package(brpc REQUIRED)

# 链接第三方库
target_link_libraries(benchmark_app
    gflags
    ${Protobuf_LIBRARIES}
    brpc
)
