#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$ROOT/build"

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[0;33m"
RESET="\033[0m"

mkdir -p "$OUT_DIR"

suite_start_ns="$(date +%s%N)"
passed=0
failed=0

list_tests() {
  find "$ROOT/testing/code" -type f -name '*.ion' \
    -not -path "$ROOT/testing/code/modules/*" \
    -not -path "$ROOT/testing/code/std/*" \
    | sort
}

total="$(list_tests | wc -l | tr -d ' ')"

print_header() {
  printf "${GREEN} %-8s %-6s %-60s %10s %-14s${RESET}\n" "PROGRESS" "RESULT" "FILE" "TIME(s)" "NOTE"
  printf "${GREEN} %-8s %-6s %-60s %10s %-14s${RESET}\n" "--------" "------" "------------------------------------------------------------" "---------" "--------------"
}

print_run_line() {
  local idx="$1" total="$2" rel="$3"
  printf "\033[0;37m[%3d/%3d] %-6s %-58s %1s %-14s\r\033[0m" "$idx" "$total" "RUN" "$rel" "" ""
}

print_result_line() {
  local idx="$1" total="$2" result="$3" rel="$4" time_s="$5" note="$6"
  printf "[%3d/%3d] %-6b   %-60s %10s %-14s\n" "$idx" "$total" "$result" "$rel" "$time_s" "$note"
}

run_test() {
  local ion_file="$1" idx="$2" total="$3"
  local name rel note time_s
  local expected actual compile_err expected_err

  name="$(basename "$ion_file" .ion)"
  rel="${ion_file#$ROOT/}"
  note=""
  time_s="-"

  print_run_line "$idx" "$total" "$rel"

  compile_err="$OUT_DIR/$name.err"
  expected_err="$ROOT/testing/stderr/$name.err"

  if ! "$ROOT/build/ionc" "$ion_file" -o "$OUT_DIR/$name.wat" 2> "$compile_err"; then
    if [ -f "$expected_err" ]; then
      if diff -u "$expected_err" "$compile_err" > /dev/null; then
        note="expected error"
        print_result_line "$idx" "$total" "${GREEN}PASS${RESET}" "$rel" "$time_s" "$note"
        return 0
      fi
      note="error mismatch"
    else
      note="compile error"
    fi
    cat "$compile_err"
    print_result_line "$idx" "$total" "${RED}FAIL${RESET}" "$rel" "$time_s" "$note"
    return 1
  fi

  if [ -f "$expected_err" ]; then
    note="expected error"
    print_result_line "$idx" "$total" "${RED}FAIL${RESET}" "$rel" "$time_s" "$note"
    return 1
  fi

  expected="$ROOT/testing/stdout/$name.out"
  actual="$OUT_DIR/$name.out"

  local -a args=()
  local stdin_file="$ROOT/testing/stdin/$name.in"
  if [ -f "$stdin_file" ]; then
    local args_content
    args_content="$(tr '\n' ' ' < "$stdin_file")"
    if [ -n "$args_content" ]; then
      read -r -a args <<<"$args_content"
    fi
  fi

  local start_ns end_ns elapsed_ns
  start_ns="$(date +%s%N)"
  local -a run_cmd
  if [ "${#args[@]}" -eq 0 ]; then
    run_cmd=(wasmtime "$OUT_DIR/$name.wat")
  else
    run_cmd=(wasmtime "$OUT_DIR/$name.wat" "${args[@]}")
  fi

  if ! "${run_cmd[@]}" > "$actual"; then
    end_ns="$(date +%s%N)"
    elapsed_ns=$((end_ns - start_ns))
    time_s="$(awk "BEGIN {printf \"%.3f\", $elapsed_ns/1000000000}")"
    note="runtime error"
    print_result_line "$idx" "$total" "${RED}FAIL${RESET}" "$rel" "$time_s" "$note"
    return 1
  fi

  end_ns="$(date +%s%N)"
  elapsed_ns=$((end_ns - start_ns))
  time_s="$(awk "BEGIN {printf \"%.3f\", $elapsed_ns/1000000000}")"

  if diff -u "$expected" "$actual" > /dev/null; then
    note="ok"
    print_result_line "$idx" "$total" "${GREEN}PASS${RESET}" "$rel" "$time_s" "$note"
    return 0
  fi

  note="output mismatch"
  print_result_line "$idx" "$total" "${RED}FAIL${RESET}" "$rel" "$time_s" "$note"
  return 1
}

print_header

idx=0
while IFS= read -r ion_file; do
  [ -z "$ion_file" ] && continue
  idx=$((idx + 1))
  if run_test "$ion_file" "$idx" "$total"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
  fi
done < <(list_tests)

echo -e "${YELLOW}Summary${RESET}: $passed passed, $failed failed, $total total"
suite_end_ns="$(date +%s%N)"
suite_elapsed_ns=$((suite_end_ns - suite_start_ns))
suite_time="$(awk "BEGIN {printf \"%.3f\", $suite_elapsed_ns/1000000000}")"
echo -e "${YELLOW}Total time${RESET}: ${suite_time}s"

if [ "$failed" -ne 0 ]; then
  exit 1
fi
