#ifndef EC20MANAGER_H
#define EC20MANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QProcess>
#include <QMutex>
#include <QVector>
#include <QTimer>
#include <QStringList>

struct SmsEntry {
    int index = -1;
    QString stat; // "REC UNREAD", "REC READ", "STO UNSENT", "STO SENT", "ALL"
    QString sender;
    QString timestamp;
    QString content;
};

class EC20Manager : public QObject
{
    Q_OBJECT

public:
    EC20Manager(const EC20Manager &)            = delete;
    EC20Manager &operator=(const EC20Manager &) = delete;

    static EC20Manager *instance();
    ~EC20Manager() override;

    // ==================== 初始化与基础操作 ====================
    bool init(const QString &atPort = QString());
    bool isOnline();
    bool getModuleInfo(QString &manufacturer, QString &model, QString &revision);
    QString getImei();
    QString getIccid();

    // ==================== SIM / 网络 / 信号 ====================
    enum SimStatus { SimUnknown,
                     SimReady,
                     SimPinRequired,
                     SimPukRequired,
                     SimNotInserted };
    enum NetworkStatus { NetNotRegistered     = 0,
                         NetRegisteredHome    = 1,
                         NetSearching         = 2,
                         NetDenied            = 3,
                         NetUnknown           = 4,
                         NetRegisteredRoaming = 5 };
    SimStatus getSimStatus();
    NetworkStatus getNetworkRegistration();
    bool getSignalQuality(int &rssi, int &ber);

    // ==================== 4G 网络连接（两种方式） ====================
    bool startQuectelCM();
    void stopQuectelCM();
    bool startPppd(const QString &operatorScript = "");
    void stopPppd();
    bool isDataConnected();
    QString getLocalIp();

    // ==================== 短信功能 ====================
    bool setSmsTextMode(bool textMode = true);
    bool sendSms(const QString &number, const QString &message);
    bool readMessage(int index, QString &sender, QString &timestamp, QString &content);
    bool deleteMessage(int index);
    QVector<SmsEntry> listMessages(const QString &stat = "ALL");

    // ==================== GPS / GNSS 定位 ====================
    bool startGps(const QString &nmeaPort = QString());
    bool stopGps();
    bool isGpsFixed() const { return m_gpsFixed; }
    bool isGpsStarted() const { return m_gpsStarted; }
    bool queryLocationOnce(double &longitude, double &latitude);

    // ==================== GPS Mock 模式（室内调试用） ====================
    bool loadNmeaFile(const QString &path);  // 加载预录 NMEA 日志文件
    bool startMockGps(int intervalMs = 200); // 回放 NMEA 数据
    void stopMockGps();                      // 停止回放
    bool isMockActive() const { return m_mockTimer != nullptr && m_mockTimer->isActive(); }

    // ==================== 底层 AT 指令 ====================
    QString sendAtCommand(const QString &cmd, int timeoutMs = 1000);
    bool sendAtCommand(const QString &cmd, QString &response, int timeoutMs = 1000);

signals:
    void dataConnectionStatusChanged(bool connected, const QString &ip = QString());
    void smsReceived(int index);
    void messageSent(bool success, const QString &errorMsg = QString());
    void messageRead(int index, const QString &sender, const QString &timestamp, const QString &content);
    void gpsStatusChanged(bool fixed);
    void positionUpdated(double longitude, double latitude);
    void newNmeaSentence(const QString &sentence);
    void errorOccurred(const QString &error);
    void signalQualityUpdated(int rssi, int ber);
    void networkRegistrationChanged(int status);

private slots:
    void onReadyRead();
    void onNmeaReadyRead();
    void onPppProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onQuectelCMMonitor();
    void onQuectelCMFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onMockTimerTick();

private:
    explicit EC20Manager(QObject *parent = nullptr);

    bool openPort();
    void closePort();
    bool openNmeaPort(const QString &port);
    void closeNmeaPort();
    void parseNmea(const QString &sentence);
    bool parseQGPSLOC(const QString &response, double &lon, double &lat);
    double convertToDecimalDegrees(const QString &nmeaCoord, bool isLat);
    bool setNmeaOutput(bool enable);

    QSerialPort m_serial;     // AT 指令端口 (ttyUSB2)
    QSerialPort m_nmeaSerial; // GPS NMEA 数据端口 (ttyUSB1)
    QByteArray m_buffer;
    QMutex m_serialMutex;
    bool m_inCommand = false; // 同步 AT 命令进行中时屏蔽 onReadyRead

    bool m_gpsFixed             = false;
    bool m_gpsStarted            = false;
    QProcess *m_quectelProcess   = nullptr;
    bool m_quectelStopping        = false;
    QProcess *m_pppProcess = nullptr;
    QString m_atPort;

    // Mock GPS 回放
    QTimer *m_mockTimer = nullptr;
    QStringList m_mockNmeaLines;
    int m_mockNmeaIndex = 0;
};

#endif // EC20MANAGER_H
