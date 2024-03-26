/**********
 * demo
 * 计划提供两个接口
 * 1. chat 接口，提供模拟 dag 流程的执行 demo
 * 2. trace 接口，提供实时查看执行流的调试功能
 * **********
*/
#include "include/stream-dag.h"

#include <gflags/gflags.h>
// #include <fmt/format.h>


#include "source.h"
#include "safety.h"
#include "llm_model.h"
#include "output.h"

DEFINE_string(input, "", "input file");
DEFINE_int64(loop_cnt, 1000, "loop count");
DEFINE_bool(trace, true, "enable trace");
DEFINE_bool(dump, false, "dump running info to local file");

DEFINE_bool(only_graph, false, " 只运行构建图");
DEFINE_bool(only_excute, false, " 图只构建一次，只运行图");
DEFINE_bool(both_run, false, "  创建图并运行");

DEFINE_bool(paralize_exe, true, " 多个图并行执行");

using namespace stream_dag;
using json = nlohmann::json;


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

int only_graph() {
    auto t1 = std::chrono::high_resolution_clock::now();    
    for (int i = 0; i < FLAGS_loop_cnt; i++) {
        StreamGraph g;
        Source* source = g.add_node<Source>("source_node");
        Split* split = g.add_node<Split>("split_node");
        PreSafety* safe_node = g.add_node<PreSafety>("safe_node");
        LLMModel* model_node = g.add_node<LLMModel>("model_node");
        OutputNode* output_node = g.add_node<OutputNode>("output_node");

        // g.add_node_dep(split, source, ASYNC_DEPENTENCY);

        class MyContext : public BaseContext {
        public:
            ChatRequest req;
        };
        // g.add_node_dep<MyContext>(output_node, {safe_node}, [](MyContext& ctx) -> bool { return ctx.req.msg.size() > 0; });

        g.add_edge_dep(split->in, source->input);
        g.add_edge_dep(safe_node->start, split->out1);
        g.add_edge_dep(model_node->req, split->out2);
        g.add_edge_dep(output_node->presafety, safe_node->out);
        g.add_edge_dep(output_node->llm_stream, model_node->rsp);
    }
   
    auto t2 = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    printf("run fin %s cost %ldns %lfms \n", "ok", cost, ms_double);
    return 0;
}

int only_excute() {
    
    // 图编排，
    StreamGraph g, g2;
    Source* source = g.add_node<Source>("source_node");
    Split* split = g.add_node<Split>("split_node");
    PreSafety* safe_node = g.add_node<PreSafety>("safe_node");
    LLMModel* model_node = g.add_node<LLMModel>("model_node");
    OutputNode* output_node = g.add_node<OutputNode>("output_node");
    g.add_edge(source->input, split->in);
    g.add_edge(split->out1, safe_node->start);
    g.add_edge(split->out2, model_node->req);
    g.add_edge(safe_node->out, output_node->presafety);
    g.add_edge(model_node->rsp, output_node->llm_stream);
    // g.dump("./graph.json");
    // g2.load("./graph.json");

    BthreadExecutor executor;

    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < FLAGS_loop_cnt; i++) {
        BaseContext ctx;
        ctx.enable_trace(FLAGS_trace);
        auto status = executor.run(g, ctx);
        if (!status.ok()) {
            printf("run err: %s\n", status.error_cstr());
            return -1;
        }
        // if (FLAGS_dump) {
        //     ctx.dump(fmt::format("running-{}.json", i));
        // }
    }
   
    auto t2 = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    printf("run fin %s cost %ldns %lfms \n", "ok", cost, ms_double);
    return 0;
}

int both_run() {
    auto t1 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < FLAGS_loop_cnt; i++) {
        StreamGraph g, g2;
        Source* source = g.add_node<Source>("source_node");
        Split* split = g.add_node<Split>("split_node");
        PreSafety* safe_node = g.add_node<PreSafety>("safe_node");
        LLMModel* model_node = g.add_node<LLMModel>("model_node");
        OutputNode* output_node = g.add_node<OutputNode>("output_node");
        g.add_edge(source->input, split->in);
        g.add_edge(split->out1, safe_node->start);
        g.add_edge(split->out2, model_node->req);
        g.add_edge(safe_node->out, output_node->presafety);
        g.add_edge(model_node->rsp, output_node->llm_stream);
        // g.dump("./graph.json");
        // g2.load("./graph.json");

        BthreadExecutor executor;
        BaseContext ctx;
        ctx.enable_trace(FLAGS_trace);
        auto status = executor.run(g, ctx);
        if (!status.ok()) {
            printf("run err: %s\n", status.error_cstr());
            return -1;
        }
        // if (FLAGS_dump) {
        //     ctx.dump(fmt::format("running-{}.json", i));
        // }
    }
   
    auto t2 = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    printf("run fin %s cost %ldns %lfms \n", "ok", cost, ms_double);
    return 0;
}

int paralize_exe() {
    // 图编排，
    StreamGraph g, g2;
    Source* source = g.add_node<Source>("source_node");
    Split* split = g.add_node<Split>("split_node");
    PreSafety* safe_node = g.add_node<PreSafety>("safe_node");
    LLMModel* model_node = g.add_node<LLMModel>("model_node");
    OutputNode* output_node = g.add_node<OutputNode>("output_node");

    g.add_node_dep(output_node, {safe_node}, CONDITION( true ));

    g.add_edge(source->input, split->in);
    g.add_edge(split->out1, safe_node->start);
    g.add_edge(split->out2, model_node->req);
    g.add_edge(safe_node->out, output_node->presafety);
    g.add_edge(model_node->rsp, output_node->llm_stream);
    // g.dump("./graph.json");
    // g2.load("./graph.json");

    auto t1 = std::chrono::high_resolution_clock::now();
    bthread_list_t list;
    bthread_list_init(&list, FLAGS_loop_cnt, 0);
    for (int i = 0; i < FLAGS_loop_cnt; i++) {
        BThread bthrd([&g, i] {
            BaseContext ctx;
            BthreadExecutor executor(std::to_string(i));
            ctx.enable_trace(FLAGS_trace);
            auto status = executor.run(g, ctx);
            if (!status.ok()) {
                printf("run err: %s\n", status.error_cstr());
                return nullptr;
            }
            return nullptr;
        });
        bthread_list_add(&list, bthrd.get_tid());
    }

    bthread_list_join(&list);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    printf("run fin %s cost %ldns %lfms \n", "ok", cost, ms_double);
    return 0;
}

int main(int argc, char *argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_both_run) {
        return both_run();
    }
    if (FLAGS_only_graph) {
        return only_graph();
    }
    if (FLAGS_only_excute) {
        return only_excute();
    }
    if (FLAGS_paralize_exe) {
        return paralize_exe();
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    // 图编排，
    StreamGraph g, g2;
    Source* source = g.add_node<Source>("source_node");
    Split* split = g.add_node<Split>("split_node");
    PreSafety* safe_node = g.add_node<PreSafety>("safe_node");
    LLMModel* model_node = g.add_node<LLMModel>("model_node");
    OutputNode* output_node = g.add_node<OutputNode>("output_node");
    
    // g.add_node_dep(output_node, {safe_node}, DependentType::SYNC_DEPENTENCY, [](BaseContext& ctx) -> bool { return true; });
    g.add_node_dep(output_node, {safe_node}, CONDITION( true ) );

    g.add_edge(source->input, split->in);
    g.add_edge(split->out1, safe_node->start);
    g.add_edge(split->out2, model_node->req);
    g.add_edge(safe_node->out, output_node->presafety);
    g.add_edge(model_node->rsp, output_node->llm_stream);
    g.dump("./graph.json");
    g2.load("./graph.json");

    BthreadExecutor executor;

    
    for (int i = 0; i < FLAGS_loop_cnt; i++) {
        BaseContext ctx;
        ctx.enable_trace(FLAGS_trace);
        auto status = executor.run(g2, ctx);
        if (!status.ok()) {
            printf("run err: %s\n", status.error_cstr());
            return -1;
        }
        // if (FLAGS_dump) {
        //     ctx.dump(fmt::format("running-{}.json", i));
        // }
    }
   
    auto t2 = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    printf("run fin %s cost %ldns %lfms \n", "ok", cost, ms_double);
    return 0;
}
