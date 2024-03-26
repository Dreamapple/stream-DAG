import pybind

# ctx = pybind.Context()
# s = pybind.Split('split', 'split')
# s.execute(ctx)
# i1 = range(10)
# # i2, i3 = split(i1, 2)

g = pybind.load_graph("build/linux/x86_64/debug/graph.json") # graph.json用不对会 core TODO 后续添加对graph.json的校验
executor = pybind.BthreadExecutor()

ctx = pybind.BaseContext("ctx1")
ctx.enable_trace(True)
    
status = executor.run(g, ctx)
if not status.ok():
    print("run err: %s" % status.error_str())

ctx.dump("pydump.json")
