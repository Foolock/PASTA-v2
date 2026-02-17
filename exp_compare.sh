#!/bin/bash
set -euo pipefail

N_RUNS=3

# -----------------------
# Helpers
# -----------------------

ensure_header_if_empty() {
  local file="$1"
  local header="$2"
  if [[ ! -s "$file" ]]; then
    echo "$header" > "$file"
  fi
}

extract_us() {
  local output="$1"
  local key="$2"
  echo "$output" \
    | grep -E "^${key}:" \
    | sed -E "s/^${key}:[[:space:]]*([0-9]+)[[:space:]]*us.*/\1/" \
    | tail -n1
}

extract_int() {
  local output="$1"
  local key="$2"
  echo "$output" \
    | grep -E "^${key}:" \
    | sed -E "s/^${key}:[[:space:]]*([0-9]+).*/\1/" \
    | tail -n1
}

# -----------------------
# CudaFlow Benchmark
# -----------------------

run_incre_cudaflow_partition() {

  local benchmark="$1"
  local output_csv="$2"
  local exe="$3"
  local matrix_size="$4"
  local num_incre_ops="$5"

  local bench_file="../benchmarks/${benchmark}.txt"
  [[ ! -f "$bench_file" ]] && return 0

  echo "===== Cudaflow: ${benchmark} ====="

  local num_nodes=""
  local num_edges=""

  local partition_sum=0
  local construct_sum=0
  local rungraph_sum=0

  for ((i=1;i<=N_RUNS;i++)); do
    echo "  -> Run ${i}/${N_RUNS}"

    output=$(${exe} "${matrix_size}" "${num_incre_ops}" "${bench_file}")

    nodes=$(extract_int "$output" "num_nodes")
    edges=$(extract_int "$output" "num_edges")
    partition=$(extract_us "$output" "total partition runtime with cudaflow partition")
    construct=$(extract_us "$output" "total construct runtime with cudaflow partition")
    rungraph=$(extract_us "$output" "total runtime with cudaflow partition")

    if [[ -z "$partition" || -z "$construct" || -z "$rungraph" ]]; then
      echo "Parse error in cudaflow output:"
      echo "$output"
      exit 1
    fi

    num_nodes="$nodes"
    num_edges="$edges"

    partition_sum=$(echo "$partition_sum + $partition" | bc -l)
    construct_sum=$(echo "$construct_sum + $construct" | bc -l)
    rungraph_sum=$(echo "$rungraph_sum + $rungraph" | bc -l)
  done

  partition_avg=$(echo "scale=6; $partition_sum / $N_RUNS" | bc -l)
  construct_avg=$(echo "scale=6; $construct_sum / $N_RUNS" | bc -l)
  rungraph_avg=$(echo "scale=6; $rungraph_sum / $N_RUNS" | bc -l)

  echo "  >> Avg partition: $partition_avg us"
  echo "  >> Avg construct: $construct_avg us"
  echo "  >> Avg rungraph : $rungraph_avg us"

  echo "${benchmark},${num_nodes},${num_edges},${partition_avg},${construct_avg},${rungraph_avg}" >> "$output_csv"
  echo
}

# -----------------------
# Semaphore Benchmark
# -----------------------

run_incre_semaphore() {

  local benchmark="$1"
  local output_csv="$2"
  local exe="$3"
  local matrix_size="$4"
  local num_incre_ops="$5"

  local bench_file="../benchmarks/${benchmark}.txt"
  [[ ! -f "$bench_file" ]] && return 0

  echo "===== Semaphore: ${benchmark} ====="

  local num_nodes=""
  local num_edges=""

  local construct_sum=0
  local rungraph_sum=0

  for ((i=1;i<=N_RUNS;i++)); do
    echo "  -> Run ${i}/${N_RUNS}"

    output=$(${exe} "${matrix_size}" "${num_incre_ops}" "${bench_file}")

    nodes=$(extract_int "$output" "num_nodes")
    edges=$(extract_int "$output" "num_edges")
    construct=$(extract_us "$output" "total constructtime with semaphore")
    rungraph=$(extract_us "$output" "total runtime with semaphore")

    if [[ -z "$construct" || -z "$rungraph" ]]; then
      echo "Parse error in semaphore output:"
      echo "$output"
      exit 1
    fi

    num_nodes="$nodes"
    num_edges="$edges"

    construct_sum=$(echo "$construct_sum + $construct" | bc -l)
    rungraph_sum=$(echo "$rungraph_sum + $rungraph" | bc -l)
  done

  construct_avg=$(echo "scale=6; $construct_sum / $N_RUNS" | bc -l)
  rungraph_avg=$(echo "scale=6; $rungraph_sum / $N_RUNS" | bc -l)

  echo "  >> Avg construct: $construct_avg us"
  echo "  >> Avg rungraph : $rungraph_avg us"

  echo "${benchmark},${num_nodes},${num_edges},${construct_avg},${rungraph_avg}" >> "$output_csv"
  echo
}

# -----------------------
# Config
# -----------------------

EXE_CUDAFLOW="./examples/incre_cudaflow_partition"
EXE_SEMAPHORE="./examples/incre_semaphore"

MATRIX_SIZE=8
NUM_INCRE_OPS=10

CUDAFLOW_CSV="cudaflow.csv"
SEMAPHORE_CSV="semaphore.csv"

benchmarks=("tv80" "wb_dma" "ac97_ctrl" "aes_core" "des_perf" "vga_lcd")

ensure_header_if_empty "$CUDAFLOW_CSV" "benchmark,num_nodes,num_edges,partition_us,construct_us,rungraph_us"
ensure_header_if_empty "$SEMAPHORE_CSV" "benchmark,num_nodes,num_edges,construct_us,rungraph_us"

# -----------------------
# Run
# -----------------------

for b in "${benchmarks[@]}"; do
  run_incre_cudaflow_partition "$b" "$CUDAFLOW_CSV" "$EXE_CUDAFLOW" "$MATRIX_SIZE" "$NUM_INCRE_OPS"
done

for b in "${benchmarks[@]}"; do
  run_incre_semaphore "$b" "$SEMAPHORE_CSV" "$EXE_SEMAPHORE" "$MATRIX_SIZE" "$NUM_INCRE_OPS"
done

echo "All benchmarks finished."
echo "Results saved to:"
echo "  - $CUDAFLOW_CSV"
echo "  - $SEMAPHORE_CSV"

