#!/bin/bash
# tools/ghidra/run.sh <xref|dec|funcs> [addresses...] - a headless query against
# the analysed dump (ghidra_proj/RE5, imported from re5dx9.fixed.exe - see
# CLAUDE.md "Tooling notes"; "x:ADDR"/"d:ADDR" mix the two in one run). The answer
# is printed and also left in ghidra_proj/report.txt.
cd "$(dirname "$0")/../.." || exit 1
OUT="$PWD/ghidra_proj/report.txt"
MAXMEM=8G /opt/ghidra/support/analyzeHeadless ghidra_proj RE5 -process re5dx9.fixed.exe -noanalysis \
  -scriptPath tools/ghidra -postScript Report.java "$OUT" "$@" > ghidra_proj/report.log 2>&1
cat "$OUT"
