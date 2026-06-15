#ifndef GPIO_TEST_PAGE_H
#define GPIO_TEST_PAGE_H

#include <QTimer>
#include <QWidget>
#include "sysfsled.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class GpioTestPage;
}
QT_END_NAMESPACE

class GpioTestPage : public QWidget
{
    Q_OBJECT

public:
    explicit GpioTestPage(QWidget *parent = nullptr);
    ~GpioTestPage() override;

signals:
    void backButtonClicked();               // 返回按钮信号
    void statusUpdated(const QString &msg); // 状态信息

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onLedToggle(bool checked);
    void onHeartModeToggled(bool checked);
    void onFlashToggled(bool checked);
    void onFlashTimeout();
    void onBeepToggle(bool checked);
    void onBeepTrigger();
    void onClearKeyPressedNum();
    void onLedError(const QString &err);
    void onBeepError(const QString &err);

private:
    void initUI();
    void setLedState(bool on);
    void setLedTrigger(const QString &trigger);
    void setBeepState(bool on);
    void updateLedIcon();
    void updateBeepIcon();
    void updateStatus(const QString &msg);

    Ui::GpioTestPage *ui;
    QTimer *m_ledFlashTimer;
    int m_keyCount;
    bool m_currentLedState;
    bool m_beepContinuous;

    SysfsLed *m_led;  // LED 控制对象
    SysfsLed *m_beep; // 蜂鸣器控制对象
};

#endif // GPIO_TEST_PAGE_H