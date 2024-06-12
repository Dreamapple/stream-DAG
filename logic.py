

def input_params(*args):
    def decorator(func):
        def wrapper(*args, **kwargs):
            for arg in args:
                if arg not in kwargs:
                    raise TypeError("Missing required argument: " + arg)
            return func(*args, **kwargs)
        return wrapper
    return decorator

class AppService:

    def __init__(self):
        self.check_safe = SafeUserSet()
        self.safe = SafetyTask()
        self.sync_safe = SafetyTask(sync=True)

        self.story = Story([
            self.check_safe,
            self.when(self.long_prompt, self.safe, self.sync_safe),
            self.intent,
            self.when(self.need_search, self.search),
            self.safe.join,

        ])

    def long_prompt(self, request):
        return request.prompt().data().size() > 1024
    def run(self, request):



app_story = Story()

ctx = Context()
req = Request()
safe = SafetyTask()
intent = Intent()

if req.user_info().id() in safe_user_set:
    ctx["safe_user"] = True

long_prompt = req.prompt().data().size() > 1024
safe_fut = safe(request)
if not long_prompt:
    safe_result = safe_fut.get()
    if safe_result != 0:
        return Response()
intent(request)










class Context(dict):
    pass

safety_user_set2 = {
        20075, 20083, 20084, 20085, 20086, 20087, 20089, 20090, 20091, 20092, 
        20093, 20094, 20095, 20096, 20097, 20098, 20099, 20100, 20101, 20102, 
        20103, 20104, 20105, 20106, 20107, 20108, 20109, 20110, 20111, 20112, 
        20113, 200
    }

safety_task = SafetyTask()

def responser_safety_answer(ctx: Context):
    pass

def run(ctx: Context):
    req = ctx.req
    # 安全用户
    ctx.is_safety_user = req.user_info().id() in safety_user_set2

    # 前置安全
    fut = safety_task.run(ctx)
    
    fut.run(responser_safety_answer, req.prompt().data().size() > 1024)

    # 意图模块
    intent_result = intent_worker.run(ctx)

    # diff! 这里的区别是如果用户问 “天气” 不同地方天气不同，需要先询问用户地区。
    only_ask = intent_result.plan_tasks.size() == 1 and intent_result.plan_tasks[0] == "worker::pTaskAsk"

    
    // 搜索
    int32_t search_count = 0;
    bool with_search_enhance = check_search_enhance(wctx);
    bool intent_search = wctx.intent_search;

    if (with_search_enhance && intent_search && !only_ask) {
        for (const std::string& query: wctx.intent_search_query_list) {
            wctx.rsp_->mutable_answer()->mutable_search()->add_querys(query);

        ctx.try_response() # querys回包
        wctx.rsp_->mutable_answer()->clear_search();

        # 搜索
        search(ctx)

    wctx.llm_ctx.mutable_count_info()->set_search_enhance_t(utils::timestamp_ms());

    // 等待安全结束
    if (safety_bid != INVALID_BTHREAD) {
        bthread_join(safety_bid, NULL);

        safety_bid = INVALID_BTHREAD;
        wctx.prompt_safety_t = utils::timestamp_ms();
        wctx.llm_ctx.mutable_count_info()->set_prompt_safety_t(wctx.prompt_safety_t);
        if (safety_task.prompt_status != 0) {
            responser_safety_answer(wctx, safety_task.prompt_status, safety_task.session_status, safety_task.safety_answer);
            return Status(0, "ok");
        }
    }
    wctx.cost("chat_bf");

    // 设置追问状态
    wctx.rsp_->set_related_status(1);
    if (!wctx.judge_task.empty() || no_releated_prompt_intent_set.count(wctx.judge_task) > 0) {
        wctx.rsp_->set_related_status(0);
    }

    // 数学问题专门处理
    if (wctx.req_->stream() && wctx.judge_task == "数学问题") {
        return math_chat(wctx);
    }

    // 获取 worker
    wctx.llm_request_t = utils::timestamp_ms();
    worker::LLMBaseWorker* pllm_worker = pllm_chat_worker_;
    if (wctx.judge_task == "诗词创作") {
        pllm_worker = pllm_poetry_chat_worker_;
        wctx.log("poetry-chat");
    }else if (search_count > 0) {
        wctx.log("33B-32k_rag");
        pllm_worker = pllm_rag_chat_worker_;
    } else { // other
        wctx.log("default 33B-8k");
    }

    if (wctx.plan_tasks.empty()) { 
        wctx.plan_tasks.emplace_back(worker::pTaskAns);
    }

    // 记录用户历史会话的信息
    wctx.cost("get_upro");
    std::string uprofile_key = wctx.req_->session_info().id() + "_" + std::to_string(wctx.req_->user_info().id());
    llm_data::ChatUserProfile user_profile;
    Status status1 = data::UserProfile::instance().get(uprofile_key, user_profile);
    LOG(INFO) << "UserProfile get result" << status1.msg() << ", content: " << user_profile.ShortDebugString();
    wctx.cost("get_upro");

    // 从user_profile获取 ask 次数，不要连续问用户 2 次
    int32_t ask_cnt = 0;
    const llm_data::ChatSessionInfo& session_info = (*user_profile.mutable_session_info())[wctx.req_->session_info().id()];
    auto iter = session_info.plan_task_info().ptask_history().rbegin();
    for (; iter != session_info.plan_task_info().ptask_history().rend(); ++iter) {
        if (iter->find(worker::pTaskAsk) == std::string::npos) { break; }
        ++ask_cnt;
    }
    wctx.log("ask_cnt " + std::to_string(ask_cnt));

    // 设置 answer_id 
    std::string uid = wctx.req_->user_info().uid().empty() ? std::to_string(wctx.req_->user_info().id()) : wctx.req_->user_info().uid();
    wctx.rsp_->mutable_answer()->set_id(utils::GenerateID::message_id(wctx.app_type, 'M', uid, wctx.req_->session_info().id()));


    bool success = false;
    std::string plan_tasks("");
    std::string full_answer; // 多轮答案的时候调试结果
    for (int i=0, size=wctx.plan_tasks.size(); i<size; ++i) {
        std::string ptask = wctx.plan_tasks[i];
        if (ask_cnt >= 2) {
            if (i == 0) { ptask = worker::pTaskAns; }
            else { break; }
        }

        wctx.log("ptask " + ptask);
        plan_tasks.append(ptask);

        LOG(INFO) << "[chat] 4.1++++++++++++++" << ptask;
        success = true;
        wctx.ptask = ptask;
        wctx.llm_ctx.clear_answer();
        if (ptask == worker::pTaskAns) {
            LOG(INFO) << "[chat] 4.1.1 ++++++++++++++" << ptask;
            stat = pllm_worker->run(wctx);
            if (!pllm_worker->success(stat)) { success = false; }
        } else if (ptask == worker::pTaskAsk) {
            LOG(INFO) << "[chat] 4.1.2 ++++++++++++++" << ptask;
            stat = llm_ask_worker_.run(wctx);
            if (!llm_ask_worker_.success(stat)) { success = false; }
        }
        full_answer += wctx.llm_ctx.answer(); // 拼接多轮结果
        if (!success) {

            if (wctx.pstream_responser_) { 
                wctx.pstream_responser_->WriteStream<llm_proto::ChatResponse>(*wctx.rsp_);
            }
            break;
        }
    }
    data::UserProfile::instance().recorder_plan_task(wctx.req_->session_info().id(), plan_tasks, user_profile);

    wctx.cost("put_upro");
    Status status = data::UserProfile::instance().put(uprofile_key, user_profile);
    LOG(INFO) << "UserProfile put result" << status.msg() << ", content: " << user_profile.ShortDebugString();
    wctx.cost("put_upro");

    wctx.llm_ctx.set_answer(full_answer);
    return Status(0, "ok");
}





