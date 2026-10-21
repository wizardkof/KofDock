#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

# Never delete user settings and never touch Plasma's existing panels.
rm -f "$HOME/.local/bin/kofdock"
rm -f "$HOME/.local/bin/kofdock.previous"
rm -f "$HOME/.local/share/applications/org.kof.KofDock.desktop"
rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/org.kof.KofDock.desktop"
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi
echo 'KofDock desinstalado. Preferências preservadas em ~/.config/KofDock/.'
