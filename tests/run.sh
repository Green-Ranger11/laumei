#!/bin/sh
# Runs each tests/<name>/shell.qml with laumei and reports pass/fail.
#   tests/run.sh [path/to/Laumei]      (default: the build tree's app)
# A test passes when its shell exits 0 (see tests/lib/Check.qml). An optional
# tests/<name>/env holds VAR=value lines; tests/<name>/bin goes first on PATH.
cd "$(dirname "$0")" || exit 1
bin="${1:-../build/Laumei.app/Contents/MacOS/Laumei}"
[ -x "$bin" ] || { echo "no laumei binary at $bin (build first)" >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

failed=""
for dir in */; do
    name=${dir%/}
    [ -f "$name/shell.qml" ] || continue
    echo "$name"
    (
        [ -f "$name/env" ] && export $(cat "$name/env")
        [ -d "$name/bin" ] && PATH="$PWD/$name/bin:$PATH"
        export AEROSPACE_MOCK_LOG="$work/$name.log"
        # macOS has no timeout(1); perl's alarm stands in.
        exec perl -e 'alarm 30; exec @ARGV' "$bin" -p "$PWD/$name"
    ) > "$work/$name.out" 2>&1
    status=$?
    # Test lines, plus anything that stopped the shell from loading.
    sed -n 's/^qml: //p; /^file:/p; /failed to load/p' "$work/$name.out"
    [ "$status" -eq 0 ] || { echo "  exit status $status"; failed="$failed $name"; }
done

if [ -n "$failed" ]; then
    echo "FAILED:$failed"
    exit 1
fi
echo "all tests passed"
