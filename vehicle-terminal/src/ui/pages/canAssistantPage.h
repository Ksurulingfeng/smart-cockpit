#ifndef CANASSISTANTPAGE_H
#define CANASSISTANTPAGE_H

#include <QTimer>
#include <QWidget>
#include "canmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CanAssistantPage;
}
QT_END_NAMESPACE

class CanAssistantPage : public QWidget
{
    Q_OBJECT

public:
    explicit CanAssistantPage(QWidget *parent = nullptr);
    ~CanAssistantPage() override;

signals:
    void backButtonClicked(); // 返回按钮信号，供外部连接

private slots:
    // CAN 设备操作
    void onOpenDevice();
    void onCloseDevice();
    void onRefreshDeviceList();
    void onSendFrame();

    // 显示控制
    void onClearReceive();
    void onClearSend();

    // 自动发送
    void onAutoSendToggled(bool checked);
    void onAutoSendTimeout();

    // CAN 管理器回调
    void onFrameReceived(const CanManager::CanFrame &frame);
    void onFrameSent(const CanManager::CanFrame &frame);
    void onErrorOccurred(const QString &errorMsg);
    void updateStatus(const QString &status);

private:
    void initUI();                                                          // 初始化界面与连接
    void updateDeviceList();                                                // 扫描可用 CAN 设备
    void appendReceiveData(const QString &data);                            // 追加数据到接收区
    QString formatFrameForDisplay(const CanManager::CanFrame &frame) const; // 格式化显示
    void connectFrameSignals();
    void disconnectFrameSignals();

    Ui::CanAssistantPage *ui;
    CanManager *m_canManager;             // CAN 管理器
    QTimer *m_autoSendTimer;              // 自动发送定时器
    CanManager::CanFrame m_autoSendFrame; // 待自动发送的帧
    bool m_rawDataMode;                   // 原始数据显示模式
    bool m_deviceOpenedByUs = false;      // 助手是否主动打开了设备
};

#endif // CANASSISTANTPAGE_H