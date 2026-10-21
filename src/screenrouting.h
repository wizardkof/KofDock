// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

// Pure screen selection logic. Wayland placement is performed separately by
// DockController when the LayerShellQt wl_surface is (re)created.
namespace ScreenRouting {
inline QString choose(const QStringList &outputs, const QString &qtPrimary,
                      const QString &saved, const QString &sessionOverride,
                      const QString &preferredPrimary = QStringLiteral("DP-3")) {
    const QString selected = sessionOverride.isEmpty() ? saved.trimmed() : sessionOverride.trimmed();
    if (!selected.isEmpty() && selected != QStringLiteral("primary")) {
        return outputs.contains(selected) ? selected : QString();
    }
    if (outputs.contains(preferredPrimary)) return preferredPrimary;
    if (outputs.contains(qtPrimary)) return qtPrimary;
    return outputs.isEmpty() ? QString() : outputs.first();
}
} // namespace ScreenRouting
