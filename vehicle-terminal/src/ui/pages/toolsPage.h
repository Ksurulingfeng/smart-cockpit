#ifndef TOOLS_PAGE_H
#define TOOLS_PAGE_H

#include <QWidget>
#include "serialAssistantPage.h"
#include "udsDiagnosticPage.h"
#include "gpioTestPage.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ToolsPage;
}
QT_END_NAMESPACE

class ToolsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ToolsPage(QWidget *parent = nullptr);
    ~ToolsPage() override;

private slots:
    // 工具子页面
    void onSerialTestClicked();
    void onCanbusTestClicked();
    void onGpioTestClicked();

    // 空调
    void onAcToggled(bool on);
    void onFanUp();
    void onFanDown();
    void onFanSliderChanged(int value);

    // 车窗
    void onWinPressed(int direction);
    void onWinReleased();
    void onWinRepeat();
    void onWinSliderChanged(int value);
    void onWinFullOpen();
    void onWinFullClose();

    // 照明灯
    void onLedToggled(bool on);

    // CAN 反馈
    void onFanCurSpeed(int percent);
    void onWindowCurPos(int percent);

private:
    void setupConnections();
    void loadState();
    void saveState();
    void sendFanTarget(int percent);
    void sendWindowTarget(int percent);
    void sendLedState(bool on);
    void applyFanLevel();
    void updateWindowLabel();

    Ui::ToolsPage *ui;
    SerialAssistantPage *m_serialAssistantPage = nullptr;
    UdsDiagnosticPage *m_udsDiagPage = nullptr;
    GpioTestPage *m_gpioTestPage               = nullptr;

    int m_fanLevel                             = 0; // 空调风速 0-100
    bool m_acOn                                = false;
    bool m_ledOn                               = false;
    int m_windowPos                            = 0; // 车窗位置 0-100

    // 长按升降窗
    QTimer *m_winRepeatTimer = nullptr;
    int m_winRepeatDir       = 0; // 1=升窗, -1=降窗
};

#endif // TOOLS_PAGE_H
