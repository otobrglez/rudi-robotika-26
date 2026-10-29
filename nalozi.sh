#!/usr/bin/env bash
# Prevede in naloži program (sketch) na Arduino Mega.
#
# Uporaba:
#   ./nalozi.sh                 seznam programov
#   ./nalozi.sh Kaca            prevedi in naloži program Kaca
#   ./nalozi.sh Kaca -m         naloži in nato prikazuj serijski izhod (Ctrl+C za izhod)
#   ./nalozi.sh Kaca -p         samo prevedi (preveri napake), ne nalagaj
#   ./nalozi.sh -m              samo prikazuj serijski izhod

set -euo pipefail

PLOSCA="arduino:avr:mega"
HITROST=9600
MAPA="$(cd "$(dirname "$0")" && pwd)"

# arduino-cli: najprej v PATH, sicer tisti iz Arduino IDE
if command -v arduino-cli >/dev/null 2>&1; then
  CLI="arduino-cli"
else
  CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
fi
if [[ ! -x "$CLI" ]] && ! command -v "$CLI" >/dev/null 2>&1; then
  echo "Ne najdem arduino-cli. Namesti Arduino IDE ali: brew install arduino-cli" >&2
  exit 1
fi

seznam() {
  echo "Programi:"
  for f in "$MAPA"/*/*.ino; do
    ime="$(basename "$(dirname "$f")")"
    [[ "$(basename "$f" .ino)" == "$ime" ]] && echo "  $ime"
  done
}

najdi_vrata() {
  local vrata
  vrata="$("$CLI" board list 2>/dev/null | awk -v p="$PLOSCA" '$0 ~ p {print $1; exit}')"
  [[ -z "$vrata" ]] && vrata="$(ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null | head -1 || true)"
  if [[ -z "$vrata" ]]; then
    echo "Plošča ni priklopljena (ne najdem USB vrat)." >&2
    exit 1
  fi
  echo "$vrata"
}

monitor() {
  local vrata="$1"
  echo "Serijski izhod na $vrata ($HITROST baud). Ctrl+C za izhod."
  "$CLI" monitor -p "$vrata" -c baudrate="$HITROST" --quiet
}

PROGRAM=""
SAMO_PREVEDI=0
MONITOR=0
for arg in "$@"; do
  case "$arg" in
    -p) SAMO_PREVEDI=1 ;;
    -m) MONITOR=1 ;;
    -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)  PROGRAM="${arg%/}" ;;
  esac
done

if [[ -z "$PROGRAM" ]]; then
  if [[ $MONITOR -eq 1 ]]; then monitor "$(najdi_vrata)"; exit 0; fi
  seznam
  exit 0
fi

if [[ ! -f "$MAPA/$PROGRAM/$PROGRAM.ino" ]]; then
  echo "Programa '$PROGRAM' ni." >&2
  seznam
  exit 1
fi

echo "Prevajam $PROGRAM ..."
"$CLI" compile --fqbn "$PLOSCA" "$MAPA/$PROGRAM"
[[ $SAMO_PREVEDI -eq 1 ]] && exit 0

VRATA="$(najdi_vrata)"
echo "Nalagam na $VRATA ..."
"$CLI" upload --fqbn "$PLOSCA" -p "$VRATA" "$MAPA/$PROGRAM"
echo "Naloženo: $PROGRAM"

[[ $MONITOR -eq 1 ]] && monitor "$VRATA"
exit 0
