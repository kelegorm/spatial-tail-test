#!/usr/bin/env bash
set -euo pipefail

PROMPT_FILE="${1:-}"

if [[ -z "$PROMPT_FILE" || ! -f "$PROMPT_FILE" ]]; then
  echo "NO ISSUES FOUND"
  exit 0
fi

PROMPT="$(cat "$PROMPT_FILE")"

read -r -d '' WRAPPED_PROMPT <<EOF || true
You are an external code reviewer.

Review the provided changes and output only findings.
Rules:
- If there are no real issues, output exactly: NO ISSUES FOUND
- Otherwise output one finding per line
- Format each finding as: file:line - concise description
- Focus on real bugs, regressions, unsafe behavior, broken assumptions, or build issues
- Do not include praise, explanations, headers, bullets, markdown, or code fences

$PROMPT
EOF

claude -p "$WRAPPED_PROMPT"