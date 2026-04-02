#!/usr/bin/env bash

set -u

EXEC_CONSTRAINED="./examples/modify_edges_with_taskflow"
EXEC_SEQ="./examples/modify_edges_with_taskflow_seq"

BENCH_DIR="../benchmarks"
MAX_PARALLELISM=8
NUM_INCRE_ITR=10
NUM_RUNS=3
OUT_CSV="modify_edges_with_taskflow_results.csv"

BENCHMARKS=(
  "tv80"
  "wb_dma"
  # "ac97_ctrl"
  # "aes_core"
  # "des_perf"
  # "vga_lcd"
)

echo "benchmark,num_edges,num_nodes,avg_critical_path_length_original,avg_critical_path_length_constrained,taskflow_runtime_us,avg_critical_path_length_seq,taskflow_runtime_seq_us" > "$OUT_CSV"

extract_value() {
  local text="$1"
  local pattern="$2"
  echo "$text" | grep -F "$pattern" | head -n1 | awk -F': ' '{print $2}'
}

extract_runtime_us() {
  local text="$1"
  local pattern="$2"
  echo "$text" | grep -F "$pattern" | head -n1 | sed -E 's/.*: ([0-9]+)us/\1/'
}

for circuit in "${BENCHMARKS[@]}"; do
  bench_file="${BENCH_DIR}/${circuit}.txt"

  sum_cpl_orig=0
  sum_cpl_constrained=0
  sum_runtime=0
  sum_cpl_seq=0
  sum_runtime_seq=0

  num_nodes=""
  num_edges=""

  for ((run=1; run<=NUM_RUNS; run++)); do
    echo "Running ${circuit} (run ${run}/${NUM_RUNS})..."

    output_constrained=$("$EXEC_CONSTRAINED" "$MAX_PARALLELISM" "$NUM_INCRE_ITR" "$bench_file" 2>&1)
    status_constrained=$?

    if [[ $status_constrained -ne 0 ]]; then
      echo "Error: constrained executable failed on ${circuit}"
      echo "$output_constrained"
      exit 1
    fi

    output_seq=$("$EXEC_SEQ" "$MAX_PARALLELISM" "$NUM_INCRE_ITR" "$bench_file" 2>&1)
    status_seq=$?

    if [[ $status_seq -ne 0 ]]; then
      echo "Error: seq executable failed on ${circuit}"
      echo "$output_seq"
      exit 1
    fi

    echo "$output_constrained"
    echo "$output_seq"

    cur_nodes=$(extract_value "$output_constrained" "num_nodes:")
    cur_edges=$(extract_value "$output_constrained" "num_edges:")
    cur_cpl_orig=$(extract_value "$output_constrained" "avg critical path length (original):")
    cur_cpl_constrained=$(extract_value "$output_constrained" "avg critical path length (constrained):")
    cur_runtime=$(extract_runtime_us "$output_constrained" "taskflow runtime:")
    cur_cpl_seq=$(extract_value "$output_seq" "avg critical path length (seq):")
    cur_runtime_seq=$(extract_runtime_us "$output_seq" "taskflow runtime(seq):")

    if [[ -z "$cur_nodes" || -z "$cur_edges" || -z "$cur_cpl_orig" || -z "$cur_cpl_constrained" || -z "$cur_runtime" || -z "$cur_cpl_seq" || -z "$cur_runtime_seq" ]]; then
      echo "Error: failed to parse output for ${circuit}"
      echo "----- constrained output -----"
      echo "$output_constrained"
      echo "----- seq output -----"
      echo "$output_seq"
      exit 1
    fi

    num_nodes="$cur_nodes"
    num_edges="$cur_edges"

    sum_cpl_orig=$(awk "BEGIN {print $sum_cpl_orig + $cur_cpl_orig}")
    sum_cpl_constrained=$(awk "BEGIN {print $sum_cpl_constrained + $cur_cpl_constrained}")
    sum_runtime=$((sum_runtime + cur_runtime))
    sum_cpl_seq=$(awk "BEGIN {print $sum_cpl_seq + $cur_cpl_seq}")
    sum_runtime_seq=$((sum_runtime_seq + cur_runtime_seq))
  done

  avg_cpl_orig=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_orig / $NUM_RUNS}")
  avg_cpl_constrained=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_constrained / $NUM_RUNS}")
  avg_runtime=$(awk "BEGIN {printf \"%.2f\", $sum_runtime / $NUM_RUNS}")
  avg_cpl_seq=$(awk "BEGIN {printf \"%.6f\", $sum_cpl_seq / $NUM_RUNS}")
  avg_runtime_seq=$(awk "BEGIN {printf \"%.2f\", $sum_runtime_seq / $NUM_RUNS}")

  echo "${circuit},${num_edges},${num_nodes},${avg_cpl_orig},${avg_cpl_constrained},${avg_runtime},${avg_cpl_seq},${avg_runtime_seq}" >> "$OUT_CSV"
done

echo "Done. Results written to $OUT_CSV"
