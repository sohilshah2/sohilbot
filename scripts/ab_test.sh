#!/usr/bin/env bash
# Build a test binary with optional feature overrides and play vs baseline.
#
# Usage (from repo root):
#   ./scripts/ab_test.sh --disable TT
#   ./scripts/ab_test.sh --disable TT,LMR --games 200 --movetime 50 --threads 4
#   ./scripts/ab_test.sh --enable ASPIRATION --games 100
#   ./scripts/ab_test.sh --extra "-DDISABLE_TT -DSEARCH_STATS_ON"
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

GAMES=100
THREADS=4
MOVETIME=50
DISABLE=""
ENABLE=""
EXTRA=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --disable|-d)
      DISABLE="${2//,/ }"
      shift 2
      ;;
    --enable|-e)
      ENABLE="${2//,/ }"
      shift 2
      ;;
    --extra)
      EXTRA="$2"
      shift 2
      ;;
    --games|-g)
      GAMES="$2"
      shift 2
      ;;
    --threads|-t)
      THREADS="$2"
      shift 2
      ;;
    --movetime|-m)
      MOVETIME="$2"
      shift 2
      ;;
    -h|--help)
      sed -n '2,11p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 1
      ;;
  esac
done

echo "==> A/B test: DISABLE=[$DISABLE] ENABLE=[$ENABLE] EXTRA=[$EXTRA]"
echo "    games=$GAMES threads=$THREADS movetime=${MOVETIME}ms"

make ab-test \
  DISABLE="$DISABLE" \
  ENABLE="$ENABLE" \
  EXTRA_FLAGS="$EXTRA" \
  GAMES="$GAMES" \
  THREADS="$THREADS" \
  MOVETIME="$MOVETIME"
