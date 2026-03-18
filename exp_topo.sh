#!/usr/bin/env bash

set -euo pipefail

EXEC="./examples/modify_edges"
BENCH_DIR="../benchmarks"
NUM_INCRE_OPS=10
NUM_RUNS=3
OUT_CSV="modify_edges_results.csv"

BENCHMARKS=(
  "tv80"
  "wb_dma"
  # "ac97_ctrl"
  # "aes_core"
  # "des_perf"
  # "vga_lcd"
)

# write CSV header
echo "circuit,num_nodes,num_edges,num_average_bedges,incre_topo_us,full_topo_us" > "$OUT_CSV"

for circuit in "${BENCHMARKS[@]}"; do
  bench_file="${BENCH_DIR}/${circuit}.txt"

  sum_bedges=0
  sum_incre=0
  sum_full=0

  num_nodes=""
  num_edges=""

  for ((run=1; run<=NUM_RUNS; run++)); do
    echo "Running ${circuit} (run ${run}/${NUM_RUNS})..."

    output=$("$EXEC" "$NUM_INCRE_OPS" "$bench_file")

    # extract values
    cur_nodes=$(echo "$output" | grep "^num_nodes:" | awk '{print $2}')
    cur_edges=$(echo "$output" | grep "^num_edges:" | awk '{print $2}')
    cur_bedges=$(echo "$output" | grep "^average number of backward edges per iteration:" | awk '{print $8}')
    cur_incre=$(echo "$output" | grep "^time spent on incrementally maintaining topo order:" | sed -E 's/.*: ([0-9]+)us/\1/')
    cur_full=$(echo "$output" | grep "^time spent on regenerating topo order:" | sed -E 's/.*: ([0-9]+)us/\1/')

    num_nodes="$cur_nodes"
    num_edges="$cur_edges"

    sum_bedges=$(awk "BEGIN {print $sum_bedges + $cur_bedges}")
    sum_incre=$((sum_incre + cur_incre))
    sum_full=$((sum_full + cur_full))
  done

  avg_bedges=$(awk "BEGIN {printf \"%.6f\", $sum_bedges / $NUM_RUNS}")
  avg_incre=$(awk "BEGIN {printf \"%.2f\", $sum_incre / $NUM_RUNS}")
  avg_full=$(awk "BEGIN {printf \"%.2f\", $sum_full / $NUM_RUNS}")

  echo "${circuit},${num_nodes},${num_edges},${avg_bedges},${avg_incre},${avg_full}" >> "$OUT_CSV"
done

echo "Done. Results written to $OUT_CSV"
