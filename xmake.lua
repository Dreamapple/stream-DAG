-- add_rules("mode.debug", "mode.release")
add_rules("mode.debug")

set_languages("c++17")

add_requires("protobuf-cpp", "gflags", "brpc", "glog", "pybind11")

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


target("pybind")
    set_kind("shared")
    add_packages("brpc")
    add_packages("pybind11")
    add_includedirs(".")
    add_includedirs("include")
    add_defines("DAG_MODULE")
    add_files("python/pybind.cpp")
    after_build(
        function(target)
            local targetfile = target:targetfile()
            os.cp(targetfile, path.join("./", path.filename(targetfile):sub(4)))
        end
    )

-- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/linghui/.pyenv/versions/3.9.18/lib
target("pybind_test")
    set_kind("binary")
    add_packages("brpc")
    add_packages("pybind11")
    add_includedirs(".")
    add_includedirs("include")
    add_files("python/pybind.cpp")