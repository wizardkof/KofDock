#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bin_dir="${HOME}/.local/bin"
app_dir="${HOME}/.local/share/applications"


case "${1:-}" in
    --help|-h)
        echo 'Uso: ./scripts/install.sh [--autostart|--no-autostart|--check]'
        echo 'Instala no usuário, sem sudo e sem alterar o painel atual.'
        exit 0
        ;;
    --check)
        echo 'Verificando ferramentas e o ambiente:'
        for tool in cmake g++ make; do
            command -v "$tool" || true
        done
        echo "Sessão: ${XDG_SESSION_TYPE:-desconhecida}"
        echo 'Qt6, LayerShellQt e módulos QML serão verificados pelo CMake e durante a execução.'
        exit 0
        ;;
    --autostart|--no-autostart|"") ;;
    *) echo "Opção não reconhecida: $1" >&2; exit 2 ;;
esac

if [[ "${XDG_SESSION_TYPE:-}" != "wayland" ]]; then
    echo 'AVISO: KofDock só executa em KDE Plasma Wayland.' >&2
fi

# Isolated Makefiles build avoids Ninja's repeated CMake-manifest regeneration
# observed on Garuda. It never reuses the failed v0.2.1 build/ cache.
cache_root="${XDG_CACHE_HOME:-$HOME/.cache}/kofdock"
build_dir="$cache_root/build-makefiles-0.2.10-beta2"
mkdir -p "$cache_root"
# Avoid a stale or reused directory from another CMake generator.
if [[ -f "$build_dir/CMakeCache.txt" ]] && ! grep -q 'CMAKE_GENERATOR:INTERNAL=Unix Makefiles' "$build_dir/CMakeCache.txt"; then
    echo "Diretório incompatível: $build_dir" >&2
    echo 'Mova-o ou remova-o manualmente antes de continuar.' >&2
    exit 3
fi
cmake -S "$root" -B "$build_dir" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel 4
ctest --test-dir "$build_dir" --output-on-failure
# Do not touch the installed program if the link step failed.
test -x "$build_dir/kofdock"

# Qt parses and instantiates QML at runtime, not at the C++ link step.
# The original 0.2.5 passed compilation but crashed while instantiating FanStack.
# Catch the same class of error before overwriting a working executable.
if [[ "${XDG_SESSION_TYPE:-}" == "wayland" && -n "${WAYLAND_DISPLAY:-}" ]]; then
    echo 'Verificando carga real do QML na sessão Wayland (sem abrir o Dock)…'
    if command -v timeout >/dev/null 2>&1; then
        timeout --signal=TERM --kill-after=2s 15s "$build_dir/kofdock" --check-qml
    else
        "$build_dir/kofdock" --check-qml
    fi
else
    echo 'AVISO: smoke-test QML não executado (sessão Wayland indisponível).' >&2
fi
mkdir -p "$bin_dir" "$app_dir"
# The previous version may still be running. Never truncate its mapped ELF.
# Keep at most one rollback copy, then atomically replace the executable.
if [[ -f "$bin_dir/kofdock" ]]; then
    cp -p "$bin_dir/kofdock" "$bin_dir/kofdock.previous"
fi
tmp="$(mktemp "$bin_dir/.kofdock.XXXXXX")"
trap 'rm -f "$tmp"' EXIT
install -m755 "$build_dir/kofdock" "$tmp"
mv -f "$tmp" "$bin_dir/kofdock"
trap - EXIT

cat > "$app_dir/org.kof.KofDock.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=KofDock
Comment=Dock no estilo macOS para KDE Plasma Wayland
Exec=${bin_dir}/kofdock
Icon=preferences-desktop-icons
Terminal=false
Categories=Utility;Qt;KDE;
X-KDE-Wayland-Interfaces=org_kde_plasma_window_management
DESKTOP

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$app_dir" || true
fi

# The app owns the autostart entry and its persisted preference.
# Default on for a fresh installation; a saved OFF is never overridden by
# reinstalling without arguments. This CLI mode needs no running compositor.
startup_mode=--sync-autostart
case "${1:-}" in
    --autostart) startup_mode=--enable-autostart ;;
    --no-autostart) startup_mode=--disable-autostart ;;
esac
"$bin_dir/kofdock" "$startup_mode"

cat <<INFO
Instalado em: $bin_dir/kofdock
Arquivo de permissões Wayland: $app_dir/org.kof.KofDock.desktop
Iniciar: $bin_dir/kofdock
Se uma versão anterior estiver rodando, encerre-a antes de iniciar a nova.
Rollback do binário anterior: $bin_dir/kofdock.previous
Build (Makefiles): $build_dir
Remover: $root/scripts/uninstall.sh

IMPORTANTE: se os indicadores de janelas não aparecerem, encerre a sessão KDE
 e entre novamente uma vez: o KWin pode exigir nova conexão para reconhecer
a permissão window_management. Pilhas não utilizam capturas de tela.
INFO
