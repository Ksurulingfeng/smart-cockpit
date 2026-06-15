#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <QObject>
#include <QProcess>
#include <QVector>
#include <QQueue>
#include <functional>

struct WifiNetwork {
    QString ssid;
    QString service;
    bool secured     = false;
    bool connected   = false;
    bool favorite    = false;
    bool autoConnect = false;
};

class WifiManager : public QObject
{
    Q_OBJECT
public:
    static WifiManager *instance();

    WifiManager(const WifiManager &)            = delete;
    WifiManager &operator=(const WifiManager &) = delete;

    void enableWifi();
    void disableWifi();
    void scanNetworks();
    void connectNetwork(const QString &ssid, const QString &password = QString());
    void disconnect();
    void forgetNetwork(const QString &ssid);

    bool isWifiEnabled() const;
    bool isWifiAvailable() const;
    QString currentSsid() const { return m_currentSsid; }
    bool isConnected() const { return m_connected; }

signals:
    void enabledWifi();
    void disabledWifi();
    void scanFinished(const QVector<WifiNetwork> &networks);
    void connectionSuccess(const QString &ssid);
    void connectionFailed(const QString &ssid, const QString &error);
    void disconnected();
    void networkForgotten(const QString &ssid);
    void errorOccurred(const QString &error);

private:
    explicit WifiManager(QObject *parent = nullptr);
    ~WifiManager() override;

    void init();
    void runCommand(const QStringList &args,
                    std::function<void(int exitCode, QProcess::ExitStatus, const QByteArray &)> callback = nullptr);
    void dequeueNext();

    void queryTechnologies();
    void getServicePathAsync(const QString &ssid, std::function<void(const QString &)> callback);
    void getCurrentServicePathAsync(std::function<void(const QString &)> callback);
    void updateConnectionState(const QVector<WifiNetwork> &nets);

    static QVector<WifiNetwork> parseServicesOutput(const QString &output);
    bool writeWifiConfig(const QString &servicePath, const QString &ssid, const QString &password);
    void removeWifiConfig(const QString &servicePath);
    void checkConnectionStatus();

    // 命令队列
    bool m_commandRunning = false;
    QQueue<std::function<void()>> m_commandQueue;

    // 连接状态
    bool m_connected        = false;
    int m_connectRetryCount = 0;
    QString m_currentSsid;
    QString m_pendingSsid;

    // 缓存的 connmanctl technologies 查询结果
    mutable bool m_techQueried   = false;
    mutable bool m_wifiAvailable = false;
    mutable bool m_wifiPowered   = false;
};

#endif // WIFIMANAGER_H
