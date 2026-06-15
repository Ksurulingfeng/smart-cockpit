#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>
#include <QVector>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include "SwitchButton.h"
#include "wifimanager.h"
#include "backlightcontrol.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class SettingsPage;
}
QT_END_NAMESPACE

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

private slots:
    void onWifiEnabled();
    void onWifiDisabled();
    void onWifiScanFinished(const QVector<WifiNetwork> &networks);
    void onWifiConnectionSuccess(const QString &ssid);
    void onWifiConnectionFailed(const QString &ssid, const QString &error);
    void onWifiDisconnected();
    void onWifiForgotten(const QString &ssid);
    void onWifiError(const QString &error);
    void onWifiEnableToggle(bool checked);
    void onRefreshClicked();
    void onNetworkItemClicked(const QString &ssid, bool secured, bool favorite);

private:
    void setupLeftArea();
    void setupPage0();
    void setupPage1();
    void setupPage2();
    void setupPage3();
    void setupPage4();
    void setupPage5();
    void setupPage6();

    void enableTouchScrolling();
    QFrame *createGroup(const QString &title);
    void addSettingRow(QFrame *group, const QString &label, const QString &value = QString(), QWidget *customWidget = nullptr, bool lastRow = false);
    void clearLayout(QLayout *layout);
    void updateWifiList(const QVector<WifiNetwork> &networks);

    Ui::SettingsPage *ui;

    WifiManager *m_wifiManager;
    BacklightControl *m_backlight; // 背光控制对象

    // Wi-Fi 页面控件
    QVBoxLayout *m_wifiListLayout;
    SwitchButton *m_wifiEnableSwitch;
    QPushButton *m_wifiRefreshBtn;
    QLabel *m_wifiStatusLabel;
};

#endif // SETTINGS_PAGE_H