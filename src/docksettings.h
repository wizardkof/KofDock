// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>

class DockSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(qreal zoomScale READ zoomScale WRITE setZoomScale NOTIFY zoomScaleChanged)
    Q_PROPERTY(int zoomSpread READ zoomSpread WRITE setZoomSpread NOTIFY zoomSpreadChanged)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(int animationMs READ animationMs WRITE setAnimationMs NOTIFY animationMsChanged)
    Q_PROPERTY(qreal backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY backgroundOpacityChanged)
    Q_PROPERTY(QString backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QString indicatorColor READ indicatorColor WRITE setIndicatorColor NOTIFY indicatorColorChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(bool shadowEnabled READ shadowEnabled WRITE setShadowEnabled NOTIFY shadowEnabledChanged)
    Q_PROPERTY(bool borderEnabled READ borderEnabled WRITE setBorderEnabled NOTIFY borderEnabledChanged)
    Q_PROPERTY(bool showIndicators READ showIndicators WRITE setShowIndicators NOTIFY showIndicatorsChanged)
    Q_PROPERTY(bool showRunningApps READ showRunningApps WRITE setShowRunningApps NOTIFY showRunningAppsChanged)
    Q_PROPERTY(bool minimizeOnClick READ minimizeOnClick WRITE setMinimizeOnClick NOTIFY minimizeOnClickChanged)
    Q_PROPERTY(bool publishMinimizeGeometry READ publishMinimizeGeometry WRITE setPublishMinimizeGeometry NOTIFY publishMinimizeGeometryChanged)
    Q_PROPERTY(bool autoHide READ autoHide WRITE setAutoHide NOTIFY autoHideChanged)
    Q_PROPERTY(bool smartHide READ smartHide WRITE setSmartHide NOTIFY smartHideChanged)
    Q_PROPERTY(bool showBadges READ showBadges WRITE setShowBadges NOTIFY showBadgesChanged)
    Q_PROPERTY(int hideDelay READ hideDelay WRITE setHideDelay NOTIFY hideDelayChanged)
    Q_PROPERTY(int bottomGap READ bottomGap WRITE setBottomGap NOTIFY bottomGapChanged)
    Q_PROPERTY(QString screenName READ screenName WRITE setScreenName NOTIFY screenNameChanged)
    Q_PROPERTY(QStringList launchers READ launchers WRITE setLaunchers NOTIFY launchersChanged)
    Q_PROPERTY(bool fanEnabled READ fanEnabled WRITE setFanEnabled NOTIFY fanEnabledChanged)
    Q_PROPERTY(int fanDelay READ fanDelay WRITE setFanDelay NOTIFY fanDelayChanged)
    Q_PROPERTY(QString stackName READ stackName WRITE setStackName NOTIFY stackNameChanged)
    Q_PROPERTY(QStringList stackLaunchers READ stackLaunchers WRITE setStackLaunchers NOTIFY stackLaunchersChanged)

public:
    explicit DockSettings(QObject *parent = nullptr);

    int iconSize() const;
    void setIconSize(int value);
    qreal zoomScale() const;
    void setZoomScale(qreal value);
    int zoomSpread() const;
    void setZoomSpread(int value);
    int spacing() const;
    void setSpacing(int value);
    int animationMs() const;
    void setAnimationMs(int value);
    qreal backgroundOpacity() const;
    void setBackgroundOpacity(qreal value);
    QString backgroundColor() const;
    void setBackgroundColor(QString value);
    QString indicatorColor() const;
    void setIndicatorColor(QString value);
    int cornerRadius() const;
    void setCornerRadius(int value);
    bool shadowEnabled() const;
    void setShadowEnabled(bool value);
    bool borderEnabled() const;
    void setBorderEnabled(bool value);
    bool showIndicators() const;
    void setShowIndicators(bool value);
    bool showRunningApps() const;
    void setShowRunningApps(bool value);
    bool minimizeOnClick() const;
    void setMinimizeOnClick(bool value);
    bool publishMinimizeGeometry() const;
    void setPublishMinimizeGeometry(bool value);
    bool autoHide() const;
    bool smartHide() const;
    bool showBadges() const;
    void setAutoHide(bool value);
    void setSmartHide(bool value);
    void setShowBadges(bool value);
    int hideDelay() const;
    void setHideDelay(int value);
    int bottomGap() const;
    void setBottomGap(int value);
    QString screenName() const;
    void setScreenName(QString value);
    bool fanEnabled() const;
    void setFanEnabled(bool value);
    int fanDelay() const;
    void setFanDelay(int value);
    QString stackName() const;
    void setStackName(QString value);
    QStringList stackLaunchers() const;
    void setStackLaunchers(QStringList value);
    QStringList launchers() const;
    void setLaunchers(QStringList value);

Q_SIGNALS:
    void iconSizeChanged();
    void zoomScaleChanged();
    void zoomSpreadChanged();
    void spacingChanged();
    void animationMsChanged();
    void backgroundOpacityChanged();
    void backgroundColorChanged();
    void indicatorColorChanged();
    void cornerRadiusChanged();
    void shadowEnabledChanged();
    void borderEnabledChanged();
    void showIndicatorsChanged();
    void showRunningAppsChanged();
    void minimizeOnClickChanged();
    void publishMinimizeGeometryChanged();
    void autoHideChanged();
    void smartHideChanged();
    void showBadgesChanged();
    void hideDelayChanged();
    void bottomGapChanged();
    void screenNameChanged();
    void launchersChanged();
    void fanEnabledChanged();
    void fanDelayChanged();
    void stackNameChanged();
    void stackLaunchersChanged();

private:
    QSettings m_store;
};
