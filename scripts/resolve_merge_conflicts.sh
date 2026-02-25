#!/usr/bin/env bash
set -euo pipefail

PREFERENCE="${1:-theirs}"

if [[ "$PREFERENCE" != "theirs" && "$PREFERENCE" != "ours" ]]; then
  echo "Usage: $0 [theirs|ours]"
  exit 1
fi

if [[ ! -f .git/MERGE_HEAD ]]; then
  echo "No merge in progress. Run this during an active merge conflict."
  exit 1
fi

FILES=(
  "CMakeLists.txt"
  "README.md"
  "CHANGELOG.md"
)

for f in "${FILES[@]}"; do
  if git ls-files -u -- "$f" | grep -q .; then
    echo "Resolving $f using --$PREFERENCE"
    git checkout "--$PREFERENCE" -- "$f"
    git add "$f"
  else
    echo "Skipping $f (no conflict entry)"
  fi
done

echo "Done. Review with: git status && git diff --staged"
echo "Then finish merge: git commit"
