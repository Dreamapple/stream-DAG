import streamlit as st
from streamlit_timeline import st_timeline
import graphviz
from pprint import pprint
import numpy as np
import time
import json
from datetime import datetime

# todo 使用状态管理 可以跳转 https://docs.streamlit.io/library/api-reference/session-state

st.set_page_config(layout="wide")

graph = None
running_raw = None
with open("../build/linux/x86_64/debug/graph.json") as f:
    graph = json.load(f)
with open("../build/linux/x86_64/debug/running-0.json") as f:
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


    # add_radio = st.radio(
    #     "选择一个 DAG 展示的方式",
    #     ("横向展示", "输入和输出流作为边展示", "输入和输出流作为节点展示")
    # )


    # add_checkbox = st.checkbox("I agree to the terms and conditions")
    # if 1 and add_radio == "横向展示":
from graphviz import nohtml
dot = graphviz.Digraph(comment='The DAG Graph', node_attr={"rankdir": "LR", "shape": "record"})
# dot.node_attr("height", ".1")
dot.attr(rankdir='LR', size='8,5')


for node in graph["nodes"]:
    name = node["name"]
    type_ = node.get("type", "unknown")
    label = []
    label.append(f"<{name}> {name}: {type_}")
    for input_ in node.get("inputs", []):
        l = input_.split("/")[-1].strip()
        label.append(f"<{l}> {l}: INPUT")

    for output_ in node.get("outputs", []):
        l = output_.split("/")[-1].strip()
        label.append(f"<{l}> {l}: OUTPUT")

    labelstr = ' | '.join(label)
    dot.node(name, label=labelstr)

for edge in graph["edges"]:
    src = edge["from"].replace("/", ":")
    dst = edge["to"].replace("/", ":")

    dot.edge(src, dst, constraint='true', style="dotted")

for node in graph["nodes"]:
    name = node["name"]
    src = f"{name}:{name}"

    for d in node.get("depends", []):
        dep_type = d.get("type", "unknown")
        for dst in d.get("dependent_nodes", []):
            taget = f"{dst}:{dst}"
            dot.edge(taget, src, constraint='true', label=f"dep_type={dep_type}", penwidth="3")


st.graphviz_chart(dot.source)

items = [
    { "content": "2022-10-20", "start": "2022-10-06 18:49:11"},
    {"id": 2, "content": "2022-10-09", "start": "2022-10-09"},
    {"id": 3, "content": "2022-10-18", "start": "2022-10-18"},
    {"id": 4, "content": "2022-10-16", "start": "2022-10-16"},
    {"id": 5, "content": "2022-10-25", "start": "2022-10-25"},
    {"id": 6, "content": "2022-10-27", "start": "2022-10-27"},
]

groups = [
    {"id": 1, "content": "A", "nestedGroups": [2, 3]},
    {"id": 2, "content": "A-01", "height": "120px"},
    {"id": 3, "content": "B", "height": "120px"},
]
groups = []
groups_map = {}

items = []
index = 0
for node_name, node_event in running_raw["nodes"].items():
    for event in node_event:

        if node_name not in groups_map:
            groups_map[node_name] = len(groups)
            groups.append({
                "id": len(groups),
                "content": node_name,
                # "nestedGroups": [len(groups)],
                # "height": "120px",
            })
        items.append({
            "id": index,
            "title": event["event"],
            "time": event["time"] / 1000000,
            "start": datetime.fromtimestamp(event["time"] / 1000000).isoformat(),
            # "end": event["time"],
            "group": groups_map[node_name],
            # "color": "red" if node_name == node_radio else "black",
            # "className": "event"
            "type": "point",
            "content": event["event"].split("::")[-1],
        })
        index += 1

from2to_name = {edge['from']:edge['to'] for edge in graph['edges']}

for stream_name, stream_event in running_raw["streams"].items():
    for event in stream_event:
        if event["event"] not in ["PipeStreamBase::read buf", "PipeStreamBase::read wait", "PipeStreamBase::read wake"]:
            node_name = stream_name.split("/")[0]
        else:
            from_stream_name = from2to_name[stream_name]
            node_name = from_stream_name.split("/")[0]
        if node_name not in groups_map:
            groups_map[node_name] = len(groups)
            groups.append({
                "id": len(groups),
                "content": node_name,
                # "nestedGroups": [len(groups)],
                # "height": "120px",
            })
        items.append({
            "id": index,
            "title": event["event"],
            "time": event["time"] / 1000000,
            "start": datetime.fromtimestamp(event["time"] / 1000000).isoformat(),
            # "end": event["time"],
            "group": groups_map[node_name],
            # "color": "red" if node_name == node_radio else "black",
            # "className": "event"
            "type": "point",
            "content": event["event"].split("::")[-1],
        })
        index += 1


items.sort(key=lambda x: x["time"])

for i in range(len(items)):
    if i == 0: continue
    prev = items[i-1]
    this = items[i]
    if this["time"] - prev["time"] <= 0.001:
        this["time"] = prev["time"] + 0.001
        this["start"] = datetime.fromtimestamp(this["time"]).isoformat()

min_time = min(k["time"] for k in items)
max_time = max(k["time"] for k in items)

options = {
    "stack": False,
    "height": '500px',
    "min": datetime.fromtimestamp(min_time).isoformat(),                # lower limit of visible range
    "max": datetime.fromtimestamp(max_time).isoformat(),                # upper limit of visible range
    # "zoomMin": 1000,             # one day in milliseconds
    # "zoomMax": 1000 * 20     # about three months in milliseconds
  }

# https://visjs.github.io/vis-timeline/docs/timeline/
timeline = st_timeline(items, groups=groups, options=options)
st.subheader("Selected item")
st.write(timeline)


col1, col2 = st.columns(2)
with col1:
    # if add_radio == "输入和输出流作为边展示" or add_radio == "横向展示":
        node_radio = st.radio(
            "选择一个节点展示详细日志",
            ("None",) + tuple(k["name"] for k in graph["nodes"])
        )

with col2:
# if 1:
    st.write("This is the second column")

    to_map = {}  # stream的会有读取和写入的两个节点。两个节点持有一个 stream 但是命名不同。根据读名称找写名称
    for edge in graph["edges"]:
        to_map[ edge["to"] ] = edge["from"]

    # pprint(to_map)

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

