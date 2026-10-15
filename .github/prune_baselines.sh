#!/usr/bin/env bash
# prune_baselines.sh — poda .github/*-baseline.txt cuando se paga deuda.
#
#   bash .github/prune_baselines.sh [--dry-run]
#
# Mide la deuda EXACTAMENTE como la CI (mismo comando y misma extraccion de
# firmas que los gates de <workflow>), y ELIMINA de la baseline las firmas que
# ya no aparecen: la deuda pagada. Los avisos de "deuda pagada — prune" de la
# CI dejan de salir una vez podado.
#
# SOLO QUITA lineas, nunca anade: una firma fuera de baseline es una
# REGRESION que el gate bloquea en CI. Aqui se reporta (hasta 30) y se sale
# con 1 sin tocar la baseline — anadirla a mano es cosa del PR que la acepta.
# Los comentarios (#) de la baseline y el orden de las firmas se conservan;
# el CRLF se ignora al leer.
#
# La medicion fallida NO poda: si el hook/clang-format/cppcheck no llega a
# medir (binario ausente, VERIFY_ONLY que falla sin ni un aviso), se aborta
# con 1 antes de tocar nada — un vacio por fallo no es deuda pagada.
#
# Salida:
#   0  baselines podadas (o ya limpio), sin firmas nuevas
#   1  firmas NUEVAS (regresion, no anadidas) o medicion no realizada
#
# OJO: esta copia lleva dentro la tabla de mediciones de LOS DIEZ repos de la
# familia (por nombre de carpeta), para que las diez copias sean IDENTICAS y
# se pueda comprobar con un diff. Si cambias un comando de medicion en el
# workflow de un repo, cambialo TAMBIEN en la tabla de aqui: las dos
# mediciones tienen que decir lo mismo para que podar sea seguro.
# Este fichero es una copia de la familia: cambios iguales en los diez repos.

set -u

cd "$(dirname "$0")/.." || exit 1
[ -d .git ] || { echo "ERROR: $(pwd) no es la raiz de un repo git."; exit 1; }
repo=$(basename "$PWD")
dry=0
[ "${1:-}" = "--dry-run" ] && dry=1

# ── tabla de mediciones (verbatim de cada gate de CI) ────────────────────────
# fmt: hook = VERIFY_ONLY del pre-commit (todos menos ABDEep);
#      directo = git ls-files + clang-format --dry-run (ABDEep);
#      ninguno = el repo no tiene baseline de formato.
cpp_cmd=""
fmt_mode=""
case "$repo" in
  ABDAudioLab)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:BuildVersion.h' --template='{severity}|{file}|{line}|{message}' src/" ;;
  ABDCZ101)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:httplib.h' --suppress='*:CircularBuffer.h' --suppress='*:BuildVersion.h' --suppress='*:*.gen.*' --template='{severity}|{file}|{line}|{message}' Source/" ;;
  ABDJUNiO601)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:BuildVersion.h' --suppress='*:*.gen.*' --template='{severity}|{file}|{line}|{message}' Source/" ;;
  ABDMS2000)
    fmt_mode=ninguno
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:BuildVersion.h' --suppress='*:*.gen.*' --template='{severity}|{file}|{line}|{message}' Source/" ;;
  ABDNeural)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:BuildVersion.h' --template='{severity}|{file}|{line}|{message}' Source/" ;;
  ABDScope)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --template='{severity}|{file}|{line}|{message}' Source/" ;;
  ABDSharedCode)
    fmt_mode=ninguno
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem -i Scope/build --template='{severity}|{file}|{line}|{message}' AudioComparator AutoUpdater BankManager DspCore DspEffects HardwareDrivers HardwareMidiDetect LcdDisplay LutDSP MidiKeyboard Scope Segmented StudioTopology SynthCore WebView2Bridge tests visualizers" ;;
  ABDBankManager)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:*.gen.*' --template='{severity}|{file}|{line}|{message}' Source apps cpp" ;;
  ABDOmegaUnified)
    fmt_mode=hook
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --template='{severity}|{file}|{line}|{message}' engine modules host/src host/legacy" ;;
  ABDEep)
    fmt_mode=directo
    cpp_cmd="cppcheck --enable=warning,performance,portability --std=c++20 --suppress=missingIncludeSystem --suppress='*:BuildVersion.h' --suppress='*:*.gen.*' --template='{severity}|{file}|{line}|{message}' Source/" ;;
  *)
    echo "ERROR: el repo '$repo' no esta en la tabla de prune_baselines.sh."
    echo "Anade su medicion de formato y cppcheck (verbatim del gate de CI)."
    exit 1 ;;
esac

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

# ── medicion de formato (mismo piping que el gate) ───────────────────────────
medir_formato() {
  command -v clang-format >/dev/null 2>&1 || {
    echo "ERROR: clang-format no esta en el PATH: la medicion no se puede hacer."; return 1; }
  set +e
  if [ "$fmt_mode" = hook ]; then
    [ -f .githooks/pre-commit ] || { echo "ERROR: no existe .githooks/pre-commit."; return 1; }
    VERIFY_ONLY=1 bash .githooks/pre-commit > "$tmp/fmt.log" 2>&1
    codigo=$?
  else
    mapfile -t ficheros < <(git ls-files 'Source/*.h' 'Source/*.cpp' 'Source/*.hpp' 'Source/*.mm')
    printf '%s\0' "${ficheros[@]}" | xargs -0 clang-format --dry-run --Werror > "$tmp/fmt.log" 2>&1
    codigo=$?
  fi
  tr -d '\r' < "$tmp/fmt.log" > "$tmp/fmt.cr"
  avisos=$(grep -c 'code should be clang-formatted' "$tmp/fmt.cr" || true)
  # Un proceso que sale con != 0 SIN ningun aviso no es deuda: es que la
  # comprobacion no llego a hacerse. No se poda sobre un vacio.
  if [ "$codigo" -ne 0 ] && [ "$avisos" -eq 0 ]; then
    echo "ERROR: la medicion de formato fallo (codigo $codigo) sin ni un aviso."
    tail -30 "$tmp/fmt.cr"; return 1
  fi
  { grep 'code should be clang-formatted' "$tmp/fmt.cr" || true; } \
    | sed -E 's/^([^:]+):[0-9]+:[0-9]+: [a-z]+: (.*)$/\1|\2/' \
    | sort -u > "$tmp/fmt.sig"
}

# ── medicion de cppcheck (mismo comando y extraccion que el gate) ────────────
medir_cppcheck() {
  command -v cppcheck >/dev/null 2>&1 || {
    echo "ERROR: cppcheck no esta en el PATH: la medicion no se puede hacer."; return 1; }
  salida=$(bash -c "$cpp_cmd" 2>&1)
  if [ -z "$salida" ]; then
    echo "ERROR: cppcheck no ha producido ni una linea (codigo $codigo): no se puede medir."; return 1
  fi
  printf '%s\n' "$salida" | tr -d '\r' > "$tmp/cpp.cr"
  # Misma firma que el gate: severidad|archivo|mensaje, sin linea, con / en
  # los paths. awk con NF>=4 y reconstruccion de mensaje = el regex del gate.
  awk -F'|' '
    /^(warning|performance|portability|error)\|/ && NF >= 4 {
      msg = $4; for (i = 5; i <= NF; i++) msg = msg "|" $i
      gsub(/\\/, "/", $2)
      print $1 "|" $2 "|" msg
    }' "$tmp/cpp.cr" | sort -u > "$tmp/cpp.sig"
}

# ── poda: quita firmas que ya no se miden; nunca anade ──────────────────────
rc=0
podar() {  # $1=baseline  $2=firmas actuales  $3=etiqueta
  local baseline=$1 sig=$2 et=$3 bl="$tmp/bl" nuevos obsoletas antes despues
  tr -d '\r' < "$baseline" > "$bl"
  antes=$(grep -cv '^#' "$bl" || true)
  nuevos=$(grep -vxF -f "$bl" "$sig" || true)
  obsoletas=$(grep -v '^#' "$bl" | grep -vxF -f "$sig" | grep -c . || true)
  despues=$((antes - obsoletas))
  if [ "$obsoletas" -eq 0 ]; then
    echo "OK: $et: $antes firmas, ninguna obsoleta — baseline sin cambios."
  elif [ "$dry" -eq 1 ]; then
    echo "DRY-RUN: $et: se eliminarian $obsoletas firmas obsoletas ($antes -> $despues)."
  else
    # getline-by-file evita la trampa awk de NR==FNR con el primer fichero vacio.
    awk -v s="$sig" '
      BEGIN { while ((getline l < s) > 0) ok[l] = 1 }
      /^#/ { print; next }
      ($0 in ok) { print }
    ' "$bl" > "$tmp/nuevo" && mv "$tmp/nuevo" "$baseline"
    echo "PODADA: $et: $obsoletas firmas eliminadas ($antes -> $despues) en $baseline."
  fi
  if [ -n "$nuevos" ]; then
    echo "REGRESION en $et: firmas NUEVAS fuera de la baseline (NO anadidas):"
    printf '%s\n' "$nuevos" | sed -n '1,30p'
    echo "(arregla la deuda nueva o anade su firma a $baseline en el PR que la acepta)"
    rc=1
  fi
}

hay_fmt=0; hay_cpp=0
[ -f .github/clang-format-baseline.txt ] && hay_fmt=1
[ -f .github/cppcheck-baseline.txt ] && hay_cpp=1

if [ "$hay_fmt" -eq 1 ] && [ "$fmt_mode" = ninguno ]; then
  echo "ERROR: $repo tiene baseline de formato pero la tabla dice 'ninguno': actualiza la tabla."
  exit 1
fi
if [ "$hay_fmt" -eq 0 ] && [ "$fmt_mode" != ninguno ]; then
  echo "nota: $repo no tiene .github/clang-format-baseline.txt — se omite el formato."
fi

if [ "$hay_fmt" -eq 1 ]; then
  echo "== clang-format ($fmt_mode) =="
  medir_formato || exit 1
  podar .github/clang-format-baseline.txt "$tmp/fmt.sig" "clang-format"
fi
if [ "$hay_cpp" -eq 1 ]; then
  echo "== cppcheck =="
  medir_cppcheck || exit 1
  podar .github/cppcheck-baseline.txt "$tmp/cpp.sig" "cppcheck"
fi
if [ "$hay_fmt" -eq 0 ] && [ "$hay_cpp" -eq 0 ]; then
  echo "nota: $repo no tiene ninguna baseline en .github/ — nada que podar."
fi

if [ "$rc" -eq 0 ] && [ "$dry" -eq 0 ]; then
  echo "Listo. Commitea la baseline podada junto al PR que paga la deuda."
fi
exit "$rc"
