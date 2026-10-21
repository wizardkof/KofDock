// SPDX-License-Identifier: MIT
#include "docksettings.h"
#include <QtGlobal>
#include <QColor>

DockSettings::DockSettings(QObject *parent)
    : QObject(parent), m_store(QSettings::IniFormat, QSettings::UserScope, "KofDock", "KofDock") {}

#define INT_SETTING(name, Name, initial, lower, upper) \
int DockSettings::name() const { return qBound(lower, m_store.value(#name, initial).toInt(), upper); } \
void DockSettings::set##Name(int value) { \
    value = qBound(lower, value, upper); \
    if (name() == value) return; \
    m_store.setValue(#name, value); emit name##Changed(); \
}

#define REAL_SETTING(name, Name, initial, lower, upper) \
qreal DockSettings::name() const { return qBound(qreal(lower), qreal(m_store.value(#name, initial).toDouble()), qreal(upper)); } \
void DockSettings::set##Name(qreal value) { \
    value = qBound(qreal(lower), value, qreal(upper)); \
    if (qAbs(name() - value) < 0.00001) return; \
    m_store.setValue(#name, value); emit name##Changed(); \
}

#define BOOL_SETTING(name, Name, initial) \
bool DockSettings::name() const { return m_store.value(#name, initial).toBool(); } \
void DockSettings::set##Name(bool value) { \
    if (name() == value) return; \
    m_store.setValue(#name, value); emit name##Changed(); \
}

#define TEXT_SETTING(name, Name, initial) \
QString DockSettings::name() const { return m_store.value(#name, QStringLiteral(initial)).toString(); } \
void DockSettings::set##Name(QString value) { \
    if (name() == value) return; \
    m_store.setValue(#name, value); emit name##Changed(); \
}

INT_SETTING(iconSize, IconSize, 52, 28, 88)
REAL_SETTING(zoomScale, ZoomScale, 1.65, 1.0, 2.1)
INT_SETTING(zoomSpread, ZoomSpread, 2, 0, 4)
INT_SETTING(spacing, Spacing, 3, 0, 24)
INT_SETTING(animationMs, AnimationMs, 145, 0, 600)
REAL_SETTING(backgroundOpacity, BackgroundOpacity, 0.56, 0.0, 1.0)
TEXT_SETTING(backgroundColor, BackgroundColor, "#222735")
TEXT_SETTING(indicatorColor, IndicatorColor, "#dcefff")
INT_SETTING(cornerRadius, CornerRadius, 19, 0, 48)
BOOL_SETTING(shadowEnabled, ShadowEnabled, false)
BOOL_SETTING(borderEnabled, BorderEnabled, false)
BOOL_SETTING(showIndicators, ShowIndicators, true)
BOOL_SETTING(showRunningApps, ShowRunningApps, true)
BOOL_SETTING(minimizeOnClick, MinimizeOnClick, false)
BOOL_SETTING(publishMinimizeGeometry, PublishMinimizeGeometry, false)
BOOL_SETTING(autoHide, AutoHide, false)
BOOL_SETTING(smartHide, SmartHide, true)
BOOL_SETTING(showBadges, ShowBadges, true)
INT_SETTING(hideDelay, HideDelay, 850, 100, 4000)
INT_SETTING(bottomGap, BottomGap, 0, 0, 32)
BOOL_SETTING(fanEnabled, FanEnabled, true)
INT_SETTING(fanDelay, FanDelay, 350, 120, 2000)
TEXT_SETTING(stackName, StackName, "Aplicativos")
TEXT_SETTING(screenName, ScreenName, "")

namespace {
QStringList cleanLauncherList(const QStringList &values) {
    QStringList clean;
    for (QString entry : values) {
        entry = entry.trimmed();
        if (entry.startsWith(QStringLiteral("applications:"))) {
            entry = entry.section(QLatin1Char('?'), 0, 0);
            QString suffix = entry.mid(QStringLiteral("applications:").size());
            while (suffix.startsWith(QLatin1Char('/'))) suffix.remove(0, 1);
            entry = QStringLiteral("applications:") + suffix;
        }
        if (!entry.isEmpty() && !clean.contains(entry)) clean.append(entry);
    }
    return clean;
}
}

QStringList DockSettings::launchers() const {
    return cleanLauncherList(m_store.value("launchers", QStringList{
        QStringLiteral("applications:org.kde.dolphin.desktop"),
        QStringLiteral("applications:org.kde.konsole.desktop"),
        QStringLiteral("applications:org.kde.systemsettings.desktop")
    }).toStringList());
}
void DockSettings::setLaunchers(QStringList value) {
    value = cleanLauncherList(value);
    if (value == launchers()) return;
    m_store.setValue("launchers", value);
    emit launchersChanged();
}

QStringList DockSettings::stackLaunchers() const {
    return m_store.value("stackLaunchers", QStringList()).toStringList();
}
void DockSettings::setStackLaunchers(QStringList value) {
    value.removeAll(QString());
    value.removeDuplicates();
    if (value == stackLaunchers()) return;
    m_store.setValue("stackLaunchers", value);
    emit stackLaunchersChanged();
}
