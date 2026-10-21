// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QRegion>
#include <QRect>
#include <QMetaObject>

class QQuickWindow;
class QScreen;
class DockSettings;

class DockController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY screenNamesChanged)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY screenGeometryChanged)
    Q_PROPERTY(QString targetScreenName READ targetScreenName NOTIFY targetScreenChanged)
public:
    explicit DockController(DockSettings *settings, QObject *parent = nullptr);
    void attachWindow(QQuickWindow *window);
    void setSessionScreenOverride(const QString &name);
    QStringList screenNames() const;
    QRect screenGeometry() const;
    QString targetScreenName() const;
    Q_INVOKABLE void selectScreen(const QString &name);
    Q_INVOKABLE void setInputArea(int x, int y, int width, int height);
    Q_INVOKABLE void setInputRegions(const QVariantList &rectangles);
    Q_INVOKABLE void resetInputArea();
Q_SIGNALS:
    void screenNamesChanged();
    void screenGeometryChanged();
    void targetScreenChanged();
private:
    void chooseScreen();
    void scheduleRemap(QScreen *target);
    void remapToPendingScreen();
    void watchScreenGeometry(QScreen *screen);
    void configureLayer();
    void applyMaskIfChanged(const QRegion &mask);
    QMetaObject::Connection m_screenGeometryConnection;
    QPointer<QScreen> m_trackedScreen;
    QPointer<QScreen> m_selectedScreen;
    QPointer<QScreen> m_pendingScreen;
    bool m_remapScheduled = false;
    bool m_remapping = false;
    QRegion m_previousMask;
    bool m_maskInitialized = false;
    QString m_sessionScreenOverride;
    DockSettings *m_settings;
    QPointer<QQuickWindow> m_window;
};
