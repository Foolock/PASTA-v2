#!/bin/bash
set -euo pipefail

# Function to run incre_cudaflow_partition benchmark and record avg runtime
run_incre_cudaflow_partition() {
  local benchmark=$1
  local output_file=$2
  local exe=$3
  local matrix_size=$4
  local num_incre_ops=$5

  # Write header if file doesn't exist / is empty
  if [[ ! -s "$output_file" ]]; then
    echo "benchmark,num_nodes,num_edges,avg_total_runtime_cudaflow_partition_ms" > "$output_file"
  fi

  local bench_file="../benchmarks/${benchmark}.txt"
  if [[ ! -f "$bench_file" ]]; then
    echo "Warning: missing benchmark file: $bench_file (skipping)" >&2
    return 0
  fi

  echo "===== Running ${benchmark} (${exe}) ====="

  local rt_sum="0.0"
  local num_nodes=""
  local num_edges=""

  for run in {1..3}; do
    echo "  -> Run ${run}/3: ${benchmark}"

    # Run executable
    output=$(${exe} "${matrix_size}" "${num_incre_ops}" "${bench_file}")

    # Parse output
    this_nodes=$(echo "$output" | grep "^num_nodes:" | sed -E 's/^num_nodes: *([0-9]+).*/\1/' | tail -n1)
    this_edges=$(echo "$output" | grep "^num_edges:" | sed -E 's/^num_edges: *([0-9]+).*/\1/' | tail -n1)
    this_rt=$(echo "$output" | grep "^total runtime with cudaflow partition:" | sed -E 's/^total runtime with cudaflow partition: *([0-9]+) *ms.*/\1/' | tail -n1)

    if [[ -z "$this_nodes" || -z "$this_edges" || -z "$this_rt" ]]; then
      echo "Error: failed to parse output for ${benchmark} (run ${run}). Full output:" >&2
      echo "$output" >&2
      exit 1
    fi

    echo "     Parsed: nodes=${this_nodes}, edges=${this_edges}, runtime=${this_rt} ms"

    num_nodes="$this_nodes"
    num_edges="$this_edges"
    rt_sum=$(echo "$rt_sum + $this_rt" | bc -l)
  done

  rt_avg=$(echo "scale=6; $rt_sum / 3.0" | bc -l)

  echo "  >> Average runtime (cudaflow partition): ${rt_avg} ms"
  echo "${benchmark},${num_nodes},${num_edges},${rt_avg}" >> "$output_file"
  echo
}

# -----------------------
# Config (easy to edit)
# -----------------------
EXE="./examples/incre_cudaflow_partition"
MATRIX_SIZE=8
NUM_INCRE_OPS=10
OUTPUT_CSV="incre_cudaflow_partition.csv"

benchmarks=("tv80" "wb_dma" "ac97_ctrl" "aes_core" "des_perf" "vga_lcd")

# Run all benchmarks
for b in "${benchmarks[@]}"; do
  run_incre_cudaflow_partition "$b" "$OUTPUT_CSV" "$EXE" "$MATRIX_SIZE" "$NUM_INCRE_OPS"
done

echo "All done! Results stored in ${OUTPUT_CSV}"

