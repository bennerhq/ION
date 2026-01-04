#!/usr/bin/env bash
# THE BEER LICENSE (with extra fizz)
#
# Author: OpenAI Codex (controlled by jens@bennerhq.com)
# This code is open source with no restrictions. Wild, right?
# If this code helps, buy Jens a beer. Or two. Or a keg.
# If it fails, keep the beer and blame the LLM gremlins.
#
# Cheers!

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$ROOT/build"

if [ $# -lt 1 ]; then
  echo "Usage: ./debug.sh <path/to/test.ion>" >&2
  exit 1
fi

ion_file="$1"
if [ ! -f "$ion_file" ]; then
  echo "File not found: $ion_file" >&2
  exit 1
fi

make -C "$ROOT"

mkdir -p "$OUT_DIR"

name="$(basename "$ion_file" .ion)"
expected="$ROOT/testing/stdout/$name.out"
actual="$OUT_DIR/$name.out"
compile_err="$OUT_DIR/$name.err"
expected_err="$ROOT/testing/stderr/$name.err"

if ! "$ROOT/build/ionc" "$ion_file" -o "$OUT_DIR/$name.wat" 2> "$compile_err"; then
  if [ -f "$expected_err" ]; then
    if diff -u "$expected_err" "$compile_err" > /dev/null; then
      echo "PASS (expected compile error)"
      exit 0
    fi
    echo "FAIL (compile error mismatch)"
    diff -u "$expected_err" "$compile_err" || true
    exit 1
  fi
  echo "FAIL (compile error)"
  cat "$compile_err"
  exit 1
fi

if [ -f "$expected_err" ]; then
  echo "FAIL (expected compile error, but compile succeeded)"
  exit 1
fi

args=()
stdin_file="$ROOT/testing/stdin/$name.in"
if [ -f "$stdin_file" ]; then
  args_content="$(tr '\n' ' ' < "$stdin_file")"
  if [ -n "$args_content" ]; then
    read -r -a args <<<"$args_content"
  fi
fi

if [ "${#args[@]}" -eq 0 ]; then
  run_cmd=(wasmtime "$OUT_DIR/$name.wat")
else
  run_cmd=(wasmtime "$OUT_DIR/$name.wat" "${args[@]}")
fi

if ! "${run_cmd[@]}" > "$actual"; then
  echo "FAIL (runtime error)"
  exit 1
fi

if [ -f "$expected" ]; then
  if diff -u "$expected" "$actual" > /dev/null; then
    echo "PASS"
  else
    echo "FAIL (output mismatch)"
    diff -u "$expected" "$actual" || true
  fi
  echo "STDOUT:"
  cat "$actual"
  if diff -u "$expected" "$actual" > /dev/null; then
    exit 0
  fi
  exit 1
fi

echo "WARN: no expected output at $expected"
echo "STDOUT:"
cat "$actual"
