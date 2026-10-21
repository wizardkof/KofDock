// SPDX-License-Identifier: MIT
#include <QtTest>
#include "screenrouting.h"

class ScreenRoutingTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void primaryUsesPreferredOutputWhenQtDisagrees() {
        const QStringList screens{QStringLiteral("HDMI-A-2"), QStringLiteral("DP-3")};
        QCOMPARE(ScreenRouting::choose(screens, QStringLiteral("HDMI-A-2"), {}, {}),
                 QStringLiteral("DP-3"));
    }
    void fallsBackToQtPrimaryWhenPreferredMissing() {
        const QStringList screens{QStringLiteral("HDMI-A-2"), QStringLiteral("DP-5")};
        QCOMPARE(ScreenRouting::choose(screens, QStringLiteral("HDMI-A-2"), {}, {}),
                 QStringLiteral("HDMI-A-2"));
    }
    void explicitOutputOverridesAuto() {
        const QStringList screens{QStringLiteral("HDMI-A-2"), QStringLiteral("DP-3")};
        QCOMPARE(ScreenRouting::choose(screens, QStringLiteral("HDMI-A-2"), {},
                                       QStringLiteral("HDMI-A-2")), QStringLiteral("HDMI-A-2"));
    }
    void startupOverrideHasPriorityOverStoredValue() {
        const QStringList screens{QStringLiteral("HDMI-A-2"), QStringLiteral("DP-3")};
        QCOMPARE(ScreenRouting::choose(screens, QStringLiteral("HDMI-A-2"),
                                       QStringLiteral("DP-3"), QStringLiteral("HDMI-A-2")),
                 QStringLiteral("HDMI-A-2"));
    }
    void invalidExplicitNameFailsClosed() {
        const QStringList screens{QStringLiteral("HDMI-A-2"), QStringLiteral("DP-3")};
        QVERIFY(ScreenRouting::choose(screens, QStringLiteral("HDMI-A-2"),
                                      QStringLiteral("DP-33"), {}).isEmpty());
    }
    void noOutputsFailsClosed() {
        QVERIFY(ScreenRouting::choose({}, {}, {}, {}).isEmpty());
    }
    void autoSelectsFirstIfQtPrimaryUnavailable() {
        const QStringList screens{QStringLiteral("DP-4")};
        QCOMPARE(ScreenRouting::choose(screens, QStringLiteral("HDMI-A-2"), {}, {}),
                 QStringLiteral("DP-4"));
    }
};

QTEST_GUILESS_MAIN(ScreenRoutingTests)
#include "test_screenrouting.moc"
