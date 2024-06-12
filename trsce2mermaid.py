# https://zhuanlan.zhihu.com/p/70261692 中文介绍不错的文档
# 直接生成的网页 https://mermaid.ink/

import json
from html import escape
trace_str = open('chat_trace.json').read()

trace = json.loads(trace_str)

def trace_to_mermaid(trace):
    result = []

    name = trace['name']
    type = trace.get('type', 'Unknown')
    for call in trace.get('call', []):
        call_name = call['name']
        call_type = call['type']
        call_args = call.get('args', [])

        args_list = []
        for arg in call_args:
            arg_name = arg['name'].replace('#', '#35;')
            if len(arg['trace']) > 0:
                arg_value = arg['trace'][0]['data']
                arg_type = arg['trace'][0]['type']
                args_list.append(f"{arg_name}: {arg_value}")
            else:
                args_list.append(f"{arg_name}: ?")
        args_str = '<br/>'.join(args_list)
        result.append(f"{name} ->> {call_name} : {args_str}")
        result.extend(trace_to_mermaid(call))

    return result

result = trace_to_mermaid(trace)
print('\n'.join(result))