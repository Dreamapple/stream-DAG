#include "include/stream-dag.h"
#include "include/http.h"
// #include "include/sinker.h"
#include <gflags/gflags.h>

using namespace stream_dag;

class SourceNode : public BaseNode {
public:
    Status run(Stream<HttpRequest>& src) {
        HttpRequest req { 
            .method = "POST",
            .url = "http://search-long-chat-service-online.inner.training-cluster.ack/generate_stream", 
            .data = json::parse(R"({"inputs":"<B_USYS>资>料：```\n《1》标题:吴华鹏_百度百科\n吴华鹏，清华大学精密仪器与机械学系学士与硕士，中国计算机用户协会常务理事，AETDEW（发展中国家工程科技院）院士, iTechClub理事长。现任秦淮数据集团董事兼首席执行官，全面负责集团战略规划、生态建设及超大规模数据中心业务发展>。\n《2》标题:吴华鹏 - 搜狗百科\n吴华鹏，1996和2000年分别获清华大学精密仪器与机械学系学士与硕士学位。曾是iTechClub互联网技术精英俱乐部及1024学院创始人，任职凤凰新媒体副总裁及凤凰网CTO，中国计算机用户协会常务理事以及发展中国家工程科技院院士。2019年加入秦>淮数据集团，任中国区总裁。现任秦淮数据CEO。中文名：吴华鹏；性别：男；职务：秦淮数据CEO；国籍：中国；就职企业：秦淮数据。在加入秦淮数据之前，吴华鹏...\n\n《3》标题:秦淮数据迎最佳领路人 原中国区总裁继任集团CEO——坚持 ...\n2月25日，秦淮数据，正式公布由原秦淮数据中国区总裁吴华鹏出任CEO。 在经历创始人居静突然卸任，网传居静与公司达成过渡性协议，此时选择由原来中国区总裁吴华鹏接任，显然是最稳妥的做法。\n《4》标题:吴华鹏出任秦淮数据CEO 将全面负责集团战略及组织管理\n吴华鹏于2019年加入秦淮数据集团，任中国区总裁。 >曾带领秦淮数据在丰富客户多样性、业务持续强劲增 长和 超前战略布局等方面取得了重大突破。 在加入秦淮数据之前，吴华鹏曾是iTechClub互联网技术精英俱乐部及1024学院创始人，任职 凤凰新媒体 副总裁及凤凰网CTO，中国计算机用户协会常务理事以及发展中国家工程科技院院士>。 董事会表示：“本次任命经过了充分的考量和全面的评估，对吴华鹏先生的战略眼光、管理经验、业务贡献和行业资源给予充分的肯定，将全力支持吴华鹏先生作为集团CEO的管理工作”。 （文猛） 关键词 : 秦淮数据 吴华鹏. 我要反馈. 新浪科技公众号. “掌”握科技鲜闻 （微信搜索techsina或扫描左侧二维码关注） 0 条评论 | 0 人参与 网友评论. 登录 | 注册.\n\n```\n今天是2024年3月21日，这是搜索增强的场景，助手需要根据资料回答用户的问题，并且根据材料作答的句子要标记引用来源、对回答中的关键信息使用**进行包裹标注。<C_Q>吴华鹏<C_A>","parameters":{"repetition_penalty":1.0499999523162842,"temperature":9.999999974752428e-7,"top_k":1,"top_p":9.999999974752428e-7,"max_new_tokens":2048,"do_sample":true,"seed":2,"details":true}})"),
            .stream = true,
        };
        src.append(req);
        return Status::OK();
    }

    DECLARE_PARAMS (
        OUTPUT(src, Stream<HttpRequest>),
    );
};
REGISTER_CLASS(SourceNode);

// class SourceNodeInitializer { public: SourceNodeInitializer() { NodeFactory::Register<SourceNode>(); } }; 

// SourceNodeInitializer _SourceNodeInitializerInstance;

class SinkerNode : public BaseNode {
public:
    Status run(Stream<HttpResponse>& result, Stream<std::string>& stream_body) {
        
        HttpResponse res;
        result.read(res);

        while(stream_body.readable()) {
            std::string body;
            stream_body.read(body);

            std::cout << body << std::endl;
        }

        return Status::OK();
    }

    DECLARE_PARAMS (
        INPUT(result, Stream<HttpResponse>),
        INPUT(stream_body, Stream<std::string>),
    );
};


REGISTER_CLASS(SinkerNode);


int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 图编排，
    json option;
    option["host"] = "http://search-long-chat-service-online.inner.training-cluster.ack/generate_stream";

    StreamGraph g{option};

    SourceNode* source = g.add_node<SourceNode>("source");
    HttpNode* http = g.add_node<HttpNode>("http_node");
    SinkerNode* sinker = g.add_node<SinkerNode>("sinker");

    http->init(option);

    g.add_edge(source->src, http->request_);
    g.add_edge(http->response_, sinker->result);
    g.add_edge(http->stream_body, sinker->stream_body);

    g.dump("./test_http2_graph.json");

    BthreadExecutor executor;

    BaseContext ctx;
    ctx.enable_trace(true);


    auto status = executor.run(g, ctx);
    if (!status.ok()) {
        printf("run err: %s\n", status.error_cstr());
        return -1;
    }

    ctx.dump("running-test_http.json");
    return 0;
}