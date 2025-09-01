#!/usr/bin/env bash
# Usage: demote_deps.sh <in.deb> [Recommends|Suggests] [regex]
# Example: demote_deps.sh build/advanced-scene-switcher_1.31.0_amd64.deb Recommends 'opencv'
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <in.deb> [Recommends|Suggests] [regex]" >&2
  exit 1
fi

IN_DEB="$1"
DEST_FIELD="${2:-Recommends}"     # Recommends or Suggests
MATCH_REGEX="${3:-opencv}"        # regex to match packages to demote

echo "Demoting dependencies matching '${MATCH_REGEX}' to ${DEST_FIELD}"

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

dpkg-deb -R "$IN_DEB" "$TMPDIR"
CONTROL="$TMPDIR/DEBIAN/control"

# Read a field (single line value), handling continuation lines starting with a space.
get_field() {
  local key="$1"
  awk -v key="$key" '
    BEGIN { val=""; collecting=0 }
    $0 ~ "^" key ":" {
      collecting=1
      sub("^" key ":[[:space:]]*", "", $0)
      val=$0
      next
    }
    collecting==1 {
      if ($0 ~ "^[[:space:]]") {
        sub("^[[:space:]]+", "", $0)
        val = val " " $0
        next
      } else {
        collecting=0
      }
    }
    END { print val }
  ' "$CONTROL"
}

# Split a comma-separated list into lines, trimming whitespace
split_csv() {
  tr ',' '\n' | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//' | sed '/^$/d'
}

# Join with ", "
join_csv() {
  awk 'BEGIN{first=1}{ if(!first) printf(", "); printf("%s",$0); first=0 } END{print ""}'
}

# Deduplicate while preserving order
dedup() {
  awk '!seen[$0]++'
}

DEPENDS_VAL="$(get_field Depends)"
EXIST_DEST_VAL="$(get_field "$DEST_FIELD")"

# Separate opencv-matching vs the rest (preserve tokens exactly)
mapfile -t DEPENDS_ARR < <(printf "%s\n" "$DEPENDS_VAL" | split_csv)
declare -a MOVED=()
declare -a KEPT=()
for item in "${DEPENDS_ARR[@]}"; do
  # Skip empty just in case
  [[ -z "$item" ]] && continue
  if [[ "$item" =~ $MATCH_REGEX ]]; then
    MOVED+=("$item")
  else
    KEPT+=("$item")
  fi
done

# Merge with existing dest field
if [[ -n "$EXIST_DEST_VAL" ]]; then
  mapfile -t EXIST_DEST_ARR < <(printf "%s\n" "$EXIST_DEST_VAL" | split_csv)
  MOVED+=("${EXIST_DEST_ARR[@]}")
fi

# De-duplicate
mapfile -t MOVED < <(printf "%s\n" "${MOVED[@]:-}" | sed '/^$/d' | dedup)
mapfile -t KEPT  < <(printf "%s\n" "${KEPT[@]:-}"  | sed '/^$/d' | dedup)

# Rebuild values
NEW_DEPENDS="$(printf "%s\n" "${KEPT[@]:-}" | join_csv || true)"
NEW_DEST="$(printf "%s\n" "${MOVED[@]:-}" | join_csv || true)"

# Write a cleaned control (remove existing Depends/Recommends/Suggests incl. continuations)
awk '
  BEGIN { skip=0 }
  {
    if ($0 ~ "^(Depends|Recommends|Suggests):") { skip=1; next }
    if (skip==1) {
      if ($0 ~ "^[[:space:]]") { next } else { skip=0 }
    }
    print
  }
' "$CONTROL" > "$CONTROL.clean"

# Append the rebuilt fields
{
  cat "$CONTROL.clean"
  if [[ -n "$NEW_DEPENDS" ]]; then
    echo "Depends: $NEW_DEPENDS"
  fi
  if [[ -n "$NEW_DEST" ]]; then
    echo "$DEST_FIELD: $NEW_DEST"
  fi
} > "$CONTROL.new"

# Clean up empty lines
sed -i '/^[[:space:]]*$/d' "$CONTROL.new"

mv "$CONTROL.new" "$CONTROL"
rm -f "$CONTROL.clean"

# Repack
rm -f "${IN_DEB}"
OUT_DEB="${IN_DEB}"
dpkg-deb -b "$TMPDIR" "$OUT_DEB" >/dev/null
echo "$OUT_DEB"
