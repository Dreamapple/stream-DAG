import streamlit as st
import graphviz
from pprint import pprint
import numpy as np
import time
import json

# todo 使用状态管理 可以跳转 https://docs.streamlit.io/library/api-reference/session-state

graph = None
running_raw = None
with open("../build/linux/x86_64/debug/graph.json") as f:
    graph = json.load(f)
with open("../build/linux/x86_64/debug/running.json") as f:
    running_raw = json.load(f)


# Using "with" notation
with st.sidebar:
    st.title("stream-DAG 可视化")
    add_input = st.text_input("搜索 request id")

    add_selectbox = st.selectbox(
        "选择一个 request id:",
        ("213-3238923-23232434343", "213-3238923-23232434344", "213-3238923-23232434345")
    )

    
    # add_number_input = st.number_input("Age")


    add_radio = st.radio(
        "选择一个 DAG 展示的方式",
        ("输入和输出流作为边展示", "输入和输出流作为节点展示")
    )

col1, col2 = st.columns(2)
with col1:
    if add_radio == "输入和输出流作为边展示":
        node_radio = st.radio(
            "选择一个节点展示详细日志",
            ("None",) + tuple(k["name"] for k in graph["nodes"])
        )

    # add_checkbox = st.checkbox("I agree to the terms and conditions")
    

    if add_radio == "输入和输出流作为节点展示":
        dot = graphviz.Digraph(comment='The DAG Graph')

        for node in graph["nodes"]:
            name = node["name"]
            shape = "box"
            dot.node(name, shape=shape) #, fontsize = "24", fontname="Helvetica,Arial,sans-serif")

            for input_ in node.get("inputs", []):
                dot.node(input_, shape="ellipse")
                dot.edge(input_, name, constraint='true')
            for output_ in node.get("outputs", []):
                dot.node(output_, shape="ellipse")
                dot.edge(name, output_,constraint='true')


        for edge in graph["edges"]:
            src = edge["from"]
            dst = edge["to"]
            shape = "ellipse"
            dot.node(src, shape=shape)
            dot.edge(src, dst,constraint='true')

        st.graphviz_chart(dot.source)

    elif add_radio == "输入和输出流作为边展示":
        dot = graphviz.Digraph(comment='The DAG Graph')

        src_node_map = {}
        dst_node_map = {}
        for node in graph["nodes"]:
            name = node["name"]
            shape = "box"
            dot.node(name, shape=shape) #, fontsize = "24", fontname="Helvetica,Arial,sans-serif")

            for input_ in node.get("inputs", []):
                dst_node_map[input_] = name
            for output_ in node.get("outputs", []):
                src_node_map[output_] = name

        for edge in graph["edges"]:
            src = edge["from"]
            dst = edge["to"]
            shape = "ellipse"
            src_node = src_node_map.pop(src)
            dst_node = dst_node_map.pop(dst)
            label = f"{src}\n{dst}"
            dot.edge(src_node, dst_node,constraint='true', label=label)

        if src_node_map:
            dot.node("end", "结束")
            for edge, node in src_node_map.items():
                dot.edge(node, "end", constraint='true', label=edge)

        st.graphviz_chart(dot.source)
    else:
        st.write(add_radio)

with col2:
# if 1:
    st.write("This is the second column")

    to_map = {}  # stream的会有读取和写入的两个节点。两个节点持有一个 stream 但是命名不同。根据读名称找写名称
    for edge in graph["edges"]:
        to_map[ edge["to"] ] = edge["from"]

    pprint(to_map)

    running = {}
    streams = running["streams"] = {}
    for stream_name, events in running_raw["streams"].items():
        streams[stream_name] = [ k["data"] for k in events if k["event"] == "PipeStreamBase::append"]

    for stream_name in to_map:
        from_stream_name = to_map[stream_name]
        streams[stream_name] = streams[from_stream_name]

    node_info_t = [k for k in graph["nodes"] if k["name"] == node_radio]
    if len(node_info_t) > 0:
        node_info = node_info_t[0]

        for input_ in node_info.get("inputs", []):

            with st.expander("Input: %s" % input_, expanded=False):
                st.json(streams[input_])

        for output_ in node_info.get("outputs", []):
            with st.expander("Output: %s" % output_, expanded=False):
                st.json(streams[output_])

        traces = []
        traces.extend(running_raw["nodes"][node_radio])
        for input_ in node_info.get("inputs", []):
            out_name_ = to_map[input_]
            traces.extend([e for e in running_raw["streams"][out_name_] if e["event"] in ["PipeStreamBase::read buf", "PipeStreamBase::read wait", "PipeStreamBase::read wake"]])

        for output_ in node_info.get("outputs", []):
            traces.extend([e for e in running_raw["streams"][output_] if e["event"] in ["PipeStreamBase::append", "PipeStreamBase::half_close"]])

        traces.sort(key=lambda x: x["time"])
        with st.expander("Running Info and Trace", expanded=False):
            st.write(traces)

