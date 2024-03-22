add_rules("mode.debug", "mode.release")

set_languages("c++17")

add_requires("protobuf-cpp", "gflags", "brpc", "glog")

target("dag-demo")
    set_kind("binary")
    add_packages("gflags")
    add_packages("glog")
    add_packages("protobuf-cpp")
    add_packages("brpc")
    add_rules("c++")
    add_includedirs("include")
    add_includedirs("workers")
    -- add_files("workers/*.cc")
    add_files("benchmark.cc")