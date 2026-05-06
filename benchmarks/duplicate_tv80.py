#!/usr/bin/env python3

import re

INPUT_FILE = "tv80.txt"
DUP_AMOUNTS = [1, 10, 50, 100, 200]

edge_re = re.compile(r'"(\d+)"\s*->\s*"(\d+)"')

with open(INPUT_FILE, "r") as f:
    lines = [line.strip() for line in f if line.strip()]

num_nodes = int(lines[0])
edges = []

for line in lines[1:]:
    m = edge_re.search(line)
    if m:
        src = int(m.group(1))
        dst = int(m.group(2))
        edges.append((src, dst))

print(f"Original nodes: {num_nodes}")
print(f"Original edges: {len(edges)}")

for dup in DUP_AMOUNTS:
    out_file = f"tv80_{dup}.txt"
    total_nodes = num_nodes * dup

    with open(out_file, "w") as out:
        out.write(f"{total_nodes}\n")

        # Write independent node names: "0", "1", ..., "total_nodes - 1"
        for i in range(total_nodes):
            out.write(f'"{i}";\n')

        # Duplicate edges without connecting copies
        for k in range(dup):
            offset = k * num_nodes
            for src, dst in edges:
                out.write(f'"{src + offset}" -> "{dst + offset}";\n')

    print(f"Generated {out_file}: {total_nodes} nodes, {len(edges) * dup} edges")
