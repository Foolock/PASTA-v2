#!/usr/bin/env bash

set -euo pipefail

EXEC_CUDAFLOW="./examples/modify_edges_with_cudaflow"
EXEC_PASTA="./examples/modify_edges_with_pasta"
EXEC_PASTA_SEQ="./examples/modify_edges_with_pasta_seq"

BENCH_DIR="../benchmarks"
MATRIX_SIZE=8
NUM_INCRE_OPS=10
NUM_RUNS=3
OUT_CSV="modify_edges_with_pasta_results.csv"

BENCHMARKS=(
  "tv80"
  "wb_dma"
  # "ac97_ctrl"
  # "aes_core"
  # "des_perf"
  # "vga_lcd"
)

extract_value() {
  local text="$1"
  local pattern="$2"
  echo "$text" | grep -F "$pattern" | head -n1 | awk -F': ' '{print $2}'
}

extract_runtime_us() {
  local text="$1"
  local pattern="$2"
  echo "$text" | grep -F "$pattern" | head -n1 | sed -E 's/.*: *([0-9]+) *us/\1/'
}

echo "benchmark,num_nodes,num_edges,critical_path_length_cudaflow,taskflow_runtime_cudaflow_us,critical_path_length_original,critical_path_length_pasta,taskflow_runtime_pasta_us,critical_path_length_seq_pasta,taskflow_runtime_seq_pasta_us" > "$OUT_CSV"

for circuit in "${BENCHMARKS[@]}"; do
  bench_file="${BENCH_DIR}/${circuit}.txt"

  sum_cpl_cudaflow=0
  sum_runtime_cudaflow=0

  sum_cpl_original=0
  sum_cpl_pasta=0
  sum_runtime_pasta=0

  sum_cpl_seq_pasta=0
  sum_runtime_seq_pasta=0

  num_nodes=""
  num_edges=""

  for ((run=1; run<=NUM_RUNS; run++)); do
    echo "Running ${circuit} (run ${run}/${NUM_RUNS})..."

    output_cudaflow=$("$EXEC_CUDAFLOW" "$MATRIX_SIZE" "$NUM_INCRE_OPS" "$bench_file" 2>&1)
    output_pasta=$("$EXEC_PASTA" "$MATRIX_SIZE" "$NUM_INCRE_OPS" "$bench_file" 2>&1)
    output_pasta_seq=$("$EXEC_PASTA_SEQ" "$MATRIX_SIZE" "$NUM_INCRE_OPS" "$bench_file" 2>&1)

    cur_nodes=$(extract_value "$output_cudaflow" "num_nodes:")
    cur_edges=$(extract_value "$output_cudaflow" "num_edges:")

    cur_cpl_cudaflow=$(extract_value "$output_cudaflow" "avg critical path length (cudaflow):")
    cur_runtime_cudaflow=$(extract_runtime_us "$output_cudaflow" "taskflow runtime (cudaflow):")

    cur_cpl_original=$(extract_value "$output_pasta" "avg critical path length (original):")
    cur_cpl_pasta=$(extract_value "$output_pasta" "avg critical path length (pasta):")
    cur_runtime_pasta=$(extract_runtime_us "$output_pasta" "taskflow runtime (pasta):")

    cur_cpl_seq_pasta=$(extract_value "$output_pasta_seq" "avg critical path length (seq pasta):")
    cur_runtime_seq_pasta=$(extract_runtime_us "$output_pasta_seq" "taskflow runtime(seq pasta):")

    if [[ -z "$cur_nodes" || -z "$cur_edges" || -z "$cur_cpl_cudaflow" || -z "$cur_runtime_cudaflow" || -z "$cur_cpl_original" || -z "$cur_cpl_pasta" || -z "$cur_runtime_pasta" || -z "$cur_cpl_seq_pasta" || -z "$cur_runtime_seq_pasta" ]]; then
      echo "Error: failed to parse output for ${circuit}"
      echo "----- cudaflow output -----"
      echo "$output_cudaflow"
      echo "----- pasta output -----"
      echo "$output_pasta"
      echo "----- pasta seq output -----"
      echo "$output_pasta_seq"
      exit 1
    fi

    num_nodes="$cur_nodes"
    num_edges="$cur_edges"

    sum_cpl_cudaflow=$(awk "BEGIN {print $sum_cpl_cudaflow + $cur_cpl_cudaflow}")
    sum_runtime_cudaflow=$((sum_runtime_cudaflow + cur_runtime_cudaflow))

    sum_cpl_original=$(awk "BEGIN {print $sum_cpl_original + $cur_cpl_original}")
    sum_cpl_pasta=$(awk "BEGIN {print $sum_cpl_pasta + $cur_cpl_pasta}")
    sum_runtime_pasta=$((sum_runtime_pasta + cur_runtime_pasta))

    sum_cpl_seq_pasta=$(awk "BEGIN {print $sum_cpl_seq_pasta + $cur_cpl_seq_pasta}")
    sum_runtime_seq_pasta=$((sum_runtime_seq_pasta + cur_runtime_seq_pasta))
  done

  avg_cpl_cudaflow=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_cudaflow / $NUM_RUNS}")
  avg_runtime_cudaflow=$(awk "BEGIN {printf \"%.2f\", $sum_runtime_cudaflow / $NUM_RUNS}")

  avg_cpl_original=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_original / $NUM_RUNS}")
  avg_cpl_pasta=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_pasta / $NUM_RUNS}")
  avg_runtime_pasta=$(awk "BEGIN {printf \"%.2f\", $sum_runtime_pasta / $NUM_RUNS}")

  avg_cpl_seq_pasta=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_seq_pasta / $NUM_RUNS}")
  avg_runtime_seq_pasta=$(awk "BEGIN {printf \"%.2f\", $sum_runtime_seq_pasta / $NUM_RUNS}")

  echo "${circuit},${num_nodes},${num_edges},${avg_cpl_cudaflow},${avg_runtime_cudaflow},${avg_cpl_original},${avg_cpl_pasta},${avg_runtime_pasta},${avg_cpl_seq_pasta},${avg_runtime_seq_pasta}" >> "$OUT_CSV"
done

echo "Done. Results written to $OUT_CSV"
