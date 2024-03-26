#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>
#include "include/common.h"
#include "include/node.h"
#include "workers/source.h"
#include "workers/safety.h"
#include "workers/llm_model.h"
#include "workers/output.h"
#include "butil/status.h"

namespace py = pybind11;
using namespace stream_dag;

int add(int i, int j) {
    return i + j;
}

class PyBaseDataWrapper : public BaseDataWrapper {
public:
    using BaseDataWrapper::BaseDataWrapper;

    std::string fullname() override {
        PYBIND11_OVERRIDE(std::string, BaseDataWrapper, fullname);
    }

    std::any create(BaseContext& context, const std::string& data_name) override {
        PYBIND11_OVERRIDE_PURE(std::any, BaseDataWrapper, create, context, data_name);
    }

    void half_close(std::any& data) override {
        PYBIND11_OVERRIDE_PURE(void, BaseDataWrapper, half_close, data);
    }
};

class PyBaseNode : public BaseNode {
public:
    using BaseNode::BaseNode;

    /* Trampoline (need one for each virtual function) */
    Status execute(BaseContext& ctx) override {
        PYBIND11_OVERRIDE_PURE(
            Status, /* Return type */
            BaseNode,      /* Parent class */
            execute,          /* Name of function in C++ (must match Python name) */
            ctx      /* Argument(s) */
        );
    }
};


class Split : public BaseNode {
public:
    Status run(Stream<Start>& in, Stream<Start>& out1, Stream<ChatRequest>& out2) {
        // for (auto& in_data: in) {
        //     out1.append(in_data);
        //     out2.append(in_data);
        // }

        while (in.readable()) {
            Start in_data;
            in.read(in_data);
            out1.append(in_data);

            ChatRequest req;
            req.msg = in_data.msg;
            out2.append(req);
        }
        return Status::OK();
    }

    DECLARE_PARAMS(
        INPUT(in, Stream<Start>),
        OUTPUT(out1, Stream<Start>),
        OUTPUT(out2, Stream<ChatRequest>),
    )
};
REGISTER_CLASS(Split);

#if defined(DAG_MODULE)
    #define _MODULE PYBIND11_MODULE
#else
    #define _MODULE PYBIND11_EMBEDDED_MODULE
#endif

_MODULE(pybind, m) {
    m.doc() = "pybind11 pybind plugin"; // optional module docstring

    m.def("add", &add, "A function which adds two numbers");

    m.def("load_graph", [](const std::string& path) {
        StreamGraph g;
        g.load(path);
        return g;
    });

    py::class_<StreamGraph>(m, "StreamGraph")
        .def(py::init<>())
        // .def("add_node", (BaseNode*(StreamGraph::*)(const std::string&)) &StreamGraph::add_node)
        // .def("add_node_dep", &StreamGraph::add_node_dep)
        // .def("add_edge", &StreamGraph::add_edge)
        // .def("list_node", &StreamGraph::list_node)
        // .def("list_depends", &StreamGraph::list_depends)
        // .def("list_edge", &StreamGraph::list_edge)
        // .def("load", &StreamGraph::load)
        .def("dump", &StreamGraph::dump);

    py::class_<BaseContext>(m, "BaseContext")
        .def(py::init<const std::string&>())
        // .def("init_data", &BaseContext::init_data)
        // .def("init_node", &BaseContext::init_node)
        // .def("init_data", (void (BaseContext::*)(const std::string&, std::any&&)) &BaseContext::init_data)
        // .def("init_input", &BaseContext::init_input)
        // .def("get_input", (template <class T> T& (BaseContext::*)(const std::string&, const std::string&)) &BaseContext::get_input)
        // .def("get_output", (template <class T> T& (BaseContext::*)(const std::string&, const std::string&)) &BaseContext::get_output)
        // .def("get_input", (template <class T> T& (BaseContext::*)(const std::string&)) &BaseContext::get_input)
        // .def("get_output", (template <class T> T& (BaseContext::*)(const std::string&)) &BaseContext::get_output)
        // .def("get_output", &BaseContext::get_output)
        // .def("get_input", &BaseContext::get_input)
        .def("enable_trace", &BaseContext::enable_trace)
        // .def("trace_node", &BaseContext::trace_node)
        // .def("trace_stream", &BaseContext::trace_stream)
        .def("dump", &BaseContext::dump);

    py::class_<BaseDataWrapper, PyBaseDataWrapper>(m, "BaseDataWrapper")
        .def(py::init<const std::string&, const std::string&>())
        .def("fullname", &BaseDataWrapper::fullname)
        .def("create", &BaseDataWrapper::create)
        .def("half_close", &BaseDataWrapper::half_close);

    py::class_<BaseNode, PyBaseNode>(m, "BaseNode")
        .def(py::init<const std::string &, const std::string &>())
        .def("name", &BaseNode::name)
        .def("execute", &BaseNode::execute)
        .def("list_input", &BaseNode::list_input)
        .def("list_output", &BaseNode::list_output)
        .def("__repr__", [](const BaseNode &n) {
            return "<BaseNode " + n.name() + ">";
        });

    py::class_<BthreadExecutor>(m, "BthreadExecutor")
        .def(py::init<>())
        .def("run", &BthreadExecutor::run, py::call_guard<py::gil_scoped_release>());

    py::class_<Split, BaseNode>(m, "Split");
        // .def("execute", &Split::execute);

    py::class_<butil::Status>(m, "Status")
        .def(py::init<>())
        .def("ok", &butil::Status::ok)
        .def("error_str", &butil::Status::error_str);

    m.def("Create", [](const std::string &name, const std::string &type) {
        return NodeFactory::CreateInstanceByName(type, name, type);
    });
    m.def("Types", []() {
        return NodeFactory::GetRegisteredTypes();
    });
}

#if !defined(DAG_MODULE)
int main() {
    py::scoped_interpreter guard{};

    py::exec(R"(
import pybind

# ctx = pybind.Context()
# s = pybind.Split('split', 'split')
# s.execute(ctx)
# i1 = range(10)
# # i2, i3 = split(i1, 2)

g = pybind.load_graph("build/linux/x86_64/debug/graph.json")
executor = pybind.BthreadExecutor()

ctx = pybind.BaseContext("ctx1")
ctx.enable_trace(True)
    
status = executor.run(g, ctx)
if not status.ok():
    print("run err: %s" % status.error_str())

ctx.dump("pydump.json")

    )", py::globals(), py::dict());
}

#endif