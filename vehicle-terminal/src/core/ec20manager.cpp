#include "ec20manager.h"
#include "configmanager.h"
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QNetworkInterface>
#include <QSerialPortInfo>
#include <QElapsedTimer>

EC20Manager *EC20Manager::instance()
{
    static EC20Manager inst;
    return &inst;
}

EC20Manager::EC20Manager(QObject *parent)
    : QObject(parent)
{
    connect(&m_serial, &QSerialPort::readyRead, this, &EC20Manager::onReadyRead);
}

EC20Manager::~EC20Manager()
{
    if (m_quectelProcess) stopQuectelCM();
    if (m_pppProcess)    stopPppd();
    if (m_gpsStarted)    stopGps();
    closeNmeaPort();
    if (m_serial.isOpen())
        m_serial.close();
}

bool EC20Manager::init(const QString &atPort)
{
    QString effectivePort = atPort;
    if (effectivePort.isEmpty()) {
        const auto ports = QSerialPortInfo::availablePorts();
        for (const auto &info : ports) {
            if (info.portName().contains("ttyUSB2")) {
                effectivePort = info.portName();
                break;
            }
        }
        if (effectivePort.isEmpty()) {
            emit errorOccurred("No EC20 AT port found (ttyUSB2 missing)");
            return false;
        }
    }

    m_atPort = effectivePort;
    m_serial.setPortName(m_atPort);
    m_serial.setBaudRate(QSerialPort::Baud115200);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!openPort())
        return false;

    if (!isOnline()) {
        emit errorOccurred("EC20 module not responding to AT");
        closePort();
        return false;
    }

    // 关闭 AT 回显，启用详细错误码
    sendAtCommand("ATE0");
    sendAtCommand("AT+CMEE=1");

    // 检查 SIM 卡状态
    SimStatus sim = getSimStatus();
    if (sim == SimNotInserted) {
        emit errorOccurred("SIM card not inserted — 网络/短信功能不可用");
        // 不阻断初始化：AT通信、模块信息、GPS 仍可正常使用
    }
    if (sim == SimPinRequired) {
        emit errorOccurred("SIM PIN required — please unlock SIM");
    }

    // 上次启用移动网络 → 启动时自动拨号
    if (ConfigManager::instance()->value("mobile_data/enabled").toBool())
        startQuectelCM();

    // 上次启用 GPS → 启动时自动开启
    if (ConfigManager::instance()->value("gps/enabled").toBool())
        startGps();

    return true;
}

bool EC20Manager::openPort()
{
    if (!m_serial.open(QIODevice::ReadWrite)) {
        emit errorOccurred(QString("Failed to open %1: %2").arg(m_atPort, m_serial.errorString()));
        return false;
    }
    return true;
}

void EC20Manager::closePort()
{
    if (m_serial.isOpen())
        m_serial.close();
}

bool EC20Manager::isOnline()
{
    QString resp = sendAtCommand("AT", 1000);
    return resp.contains("OK");
}

// ==================== SIM / 网络 / 信号 ====================

EC20Manager::SimStatus EC20Manager::getSimStatus()
{
    QString resp = sendAtCommand("AT+CPIN?");
    if (resp.contains("+CPIN: READY"))
        return SimReady;
    if (resp.contains("SIM PIN"))
        return SimPinRequired;
    if (resp.contains("SIM PUK"))
        return SimPukRequired;
    if (resp.contains("SIM not inserted") || resp.contains("ERROR"))
        return SimNotInserted;
    return SimUnknown;
}

EC20Manager::NetworkStatus EC20Manager::getNetworkRegistration()
{
    QString resp = sendAtCommand("AT+CREG?");
    QRegularExpression re("\\+CREG:\\s*\\d+,\\s*(\\d+)");
    QRegularExpressionMatch match = re.match(resp);
    if (match.hasMatch()) {
        int stat = match.captured(1).toInt();
        emit networkRegistrationChanged(stat);
        return static_cast<NetworkStatus>(stat);
    }
    return NetUnknown;
}

bool EC20Manager::getSignalQuality(int &rssi, int &ber)
{
    QString resp = sendAtCommand("AT+CSQ");
    QRegularExpression re("\\+CSQ:\\s*(\\d+),\\s*(\\d+)");
    QRegularExpressionMatch match = re.match(resp);
    if (match.hasMatch()) {
        rssi = match.captured(1).toInt();
        ber  = match.captured(2).toInt();
        emit signalQualityUpdated(rssi, ber);
        return true;
    }
    return false;
}

// ==================== 模块信息 ====================

bool EC20Manager::getModuleInfo(QString &manufacturer, QString &model, QString &revision)
{
    manufacturer = sendAtCommand("AT+CGMI").remove("OK").trimmed();
    model        = sendAtCommand("AT+CGMM").remove("OK").trimmed();
    revision     = sendAtCommand("AT+CGMR").remove("OK").trimmed();
    return !manufacturer.isEmpty() && !model.isEmpty();
}

QString EC20Manager::getImei()
{
    QString resp = sendAtCommand("AT+CGSN");
    // 去掉可能的回显和 OK
    QRegularExpression re("(\\d{15})");
    QRegularExpressionMatch match = re.match(resp);
    if (match.hasMatch())
        return match.captured(1);
    return QString();
}

QString EC20Manager::getIccid()
{
    QString resp = sendAtCommand("AT+QCCID");
    QRegularExpression re("\\+QCCID:\\s*(\\d{19,20})");
    QRegularExpressionMatch match = re.match(resp);
    if (match.hasMatch())
        return match.captured(1);
    return QString();
}

// ==================== 4G 联网：quectel-CM 方式 ====================

bool EC20Manager::startQuectelCM()
{
    QProcess pgrep;
    pgrep.start("pgrep", QStringList() << "quectel-CM");
    pgrep.waitForFinished();
    if (pgrep.exitCode() == 0) {
        qDebug() << "quectel-CM already running";
        emit dataConnectionStatusChanged(true, getLocalIp());
        return true;
    }

    if (m_quectelProcess) {
        stopQuectelCM();
    }

    m_quectelProcess = new QProcess(this);
    connect(m_quectelProcess, &QProcess::readyReadStandardOutput, this, &EC20Manager::onQuectelCMMonitor);
    connect(m_quectelProcess, &QProcess::readyReadStandardError, this, &EC20Manager::onQuectelCMMonitor);
    connect(m_quectelProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EC20Manager::onQuectelCMFinished);

    // TODO: 当前使用 QEventLoop 阻塞等待，会卡 UI 线程4秒，应改为异步信号驱动
    m_quectelProcess->start("quectel-CM", QStringList());
    if (!m_quectelProcess->waitForStarted(3000)) {
        emit errorOccurred("Failed to start quectel-CM");
        delete m_quectelProcess;
        m_quectelProcess = nullptr;
        return false;
    }

    // 等待连接建立（使用事件循环避免阻塞 UI）
    QEventLoop loop;
    QTimer::singleShot(4000, &loop, &QEventLoop::quit);
    loop.exec();
    bool connected = isDataConnected();
    if (connected) {
        emit dataConnectionStatusChanged(true, getLocalIp());
        qDebug() << "4G connected via quectel-CM, IP:" << getLocalIp();
    } else {
        emit errorOccurred("quectel-CM started but no IP obtained");
    }
    return connected;
}

void EC20Manager::stopQuectelCM()
{
    if (m_quectelProcess) {
        m_quectelStopping = true;
        disconnect(m_quectelProcess, &QProcess::readyReadStandardOutput, this, &EC20Manager::onQuectelCMMonitor);
        disconnect(m_quectelProcess, &QProcess::readyReadStandardError, this, &EC20Manager::onQuectelCMMonitor);
        m_quectelProcess->terminate();
        if (!m_quectelProcess->waitForFinished(3000))
            m_quectelProcess->kill();
        delete m_quectelProcess;
        m_quectelProcess = nullptr;
        emit dataConnectionStatusChanged(false);
        qDebug() << "quectel-CM stopped";
    }
}

void EC20Manager::onQuectelCMMonitor()
{
    QProcess *p = qobject_cast<QProcess *>(sender());
    if (!p)
        return;

    QString output = QString::fromUtf8(p->readAllStandardOutput());
    if (!output.isEmpty())
        qDebug() << "quectel-CM stdout:" << output;

    QString errOutput = QString::fromUtf8(p->readAllStandardError());
    if (!errOutput.isEmpty())
        qDebug() << "quectel-CM stderr:" << errOutput;
}

void EC20Manager::onQuectelCMFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    emit dataConnectionStatusChanged(false);

    // 主动停止 → 不重连
    if (m_quectelStopping) {
        m_quectelStopping = false;
        return;
    }

    qDebug() << "quectel-CM process exited";

    // 异常崩溃 → 自动重连
    if (m_quectelProcess && exitStatus != QProcess::NormalExit) {
        qDebug() << "quectel-CM crashed, reconnecting in 5s...";
        QTimer::singleShot(5000, this, [this]() {
            if (m_quectelProcess) startQuectelCM();
        });
    }
}

// ==================== 4G 联网：PPP 拨号方式 ====================

bool EC20Manager::startPppd(const QString &operatorScript)
{
    QString script;
    if (!operatorScript.isEmpty()) {
        script = operatorScript;
    } else {
        script = "/home/root/shell/4G/ppp-on-10000";
    }

    if (!QFile::exists(script)) {
        emit errorOccurred(QString("PPP script not found: %1").arg(script));
        return false;
    }

    if (m_pppProcess) {
        stopPppd();
    }

    m_pppProcess = new QProcess(this);
    connect(m_pppProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EC20Manager::onPppProcessFinished);

    m_pppProcess->start(script, QStringList());
    if (!m_pppProcess->waitForStarted(3000)) {
        emit errorOccurred("Failed to start PPP script");
        delete m_pppProcess;
        m_pppProcess = nullptr;
        return false;
    }

    // 等待 PPP 连接建立（使用事件循环避免阻塞 UI）
    QEventLoop loop;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    bool connected = isDataConnected();
    if (connected) {
        emit dataConnectionStatusChanged(true, getLocalIp());
        qDebug() << "PPP connected, IP:" << getLocalIp();
    } else {
        emit errorOccurred("PPP script started but no IP obtained");
    }
    return connected;
}

void EC20Manager::stopPppd()
{
    if (m_pppProcess) {
        m_pppProcess->terminate();
        if (!m_pppProcess->waitForFinished(3000))
            m_pppProcess->kill();
        delete m_pppProcess;
        m_pppProcess = nullptr;
        emit dataConnectionStatusChanged(false);
        qDebug() << "PPP stopped";
    }
}

void EC20Manager::onPppProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    qDebug() << "PPP process finished";
    emit dataConnectionStatusChanged(false);
}

bool EC20Manager::isDataConnected()
{
    const QStringList ifaces            = {"wwan0", "ppp0", "eth2", "usb0"};
    const QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();
    for (const QString &iface : ifaces) {
        for (const QNetworkInterface &net : nets) {
            if (net.name() == iface && net.flags().testFlag(QNetworkInterface::IsRunning)) {
                QList<QNetworkAddressEntry> entries = net.addressEntries();
                for (const QNetworkAddressEntry &entry : entries) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                        !entry.ip().isLoopback()) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

QString EC20Manager::getLocalIp()
{
    const QStringList ifaces            = {"wwan0", "ppp0", "eth2", "usb0"};
    const QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();
    for (const QString &iface : ifaces) {
        for (const QNetworkInterface &net : nets) {
            if (net.name() == iface && net.flags().testFlag(QNetworkInterface::IsRunning)) {
                QList<QNetworkAddressEntry> entries = net.addressEntries();
                for (const QNetworkAddressEntry &entry : entries) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                        !entry.ip().isLoopback()) {
                        return entry.ip().toString();
                    }
                }
            }
        }
    }
    return QString();
}

// ==================== 短信功能 ====================

bool EC20Manager::setSmsTextMode(bool textMode)
{
    QString cmd  = textMode ? "AT+CMGF=1" : "AT+CMGF=0";
    QString resp = sendAtCommand(cmd);
    return resp.contains("OK");
}

bool EC20Manager::sendSms(const QString &number, const QString &message)
{
    if (!setSmsTextMode(true)) {
        emit errorOccurred("Failed to set SMS text mode");
        return false;
    }

    QString cmd  = QString("AT+CMGS=\"%1\"").arg(number);
    QString resp = sendAtCommand(cmd, 2000);
    if (!resp.contains(">")) {
        emit errorOccurred("Module did not return '>' prompt");
        return false;
    }

    QByteArray smsData = message.toUtf8() + '\x1A';
    m_serial.write(smsData);
    if (!m_serial.waitForBytesWritten(2000)) {
        emit errorOccurred("Write timeout");
        return false;
    }

    QByteArray finalResp;
    if (m_serial.waitForReadyRead(20000)) {
        finalResp = m_serial.readAll();
    } else {
        emit errorOccurred("No response after sending SMS");
        return false;
    }

    if (finalResp.contains("+CMGS:")) {
        emit messageSent(true);
        return true;
    } else {
        emit messageSent(false, QString::fromUtf8(finalResp));
        return false;
    }
}

bool EC20Manager::readMessage(int index, QString &sender, QString &timestamp, QString &content)
{
    if (!setSmsTextMode(true))
        return false;

    QString cmd  = QString("AT+CMGR=%1").arg(index);
    QString resp = sendAtCommand(cmd, 3000);
    if (resp.isEmpty())
        return false;

    // +CMGR: <stat>,<oa>,<alpha>,<scts>[,<tooa>,<fo>,<pid>,<dcs>,<sca>,<tosca>,<length>]
    // <CR><LF><data>
    QRegularExpression re("\\+CMGR:\\s*\"([^\"]*)\",\"([^\"]*)\",[^,]*,\"([^\"]*)\"");
    QRegularExpressionMatch match = re.match(resp);
    if (match.hasMatch()) {
        sender    = match.captured(2);
        timestamp = match.captured(3);

        // 取 \r\n 后的内容直到结尾或 OK 之前
        int bodyStart = match.capturedEnd();
        if (bodyStart < resp.length()) {
            int okPos = resp.lastIndexOf("OK");
            if (okPos > bodyStart) {
                content = resp.mid(bodyStart, okPos - bodyStart).trimmed();
            } else {
                content = resp.mid(bodyStart).trimmed();
            }
        }

        emit messageRead(index, sender, timestamp, content);
        return true;
    }
    return false;
}

bool EC20Manager::deleteMessage(int index)
{
    QString cmd  = QString("AT+CMGD=%1").arg(index);
    QString resp = sendAtCommand(cmd);
    return resp.contains("OK");
}

QVector<SmsEntry> EC20Manager::listMessages(const QString &stat)
{
    QVector<SmsEntry> entries;

    if (!setSmsTextMode(true))
        return entries;

    QString cmd  = QString("AT+CMGL=\"%1\"").arg(stat);
    QString resp = sendAtCommand(cmd, 5000);
    if (resp.isEmpty())
        return entries;

    // 解析 +CMGL: <index>,<stat>,<oa>,<alpha>,<scts>[,<tooa>,<length>]
    // <CR><LF><data>
    QRegularExpression re("\\+CMGL:\\s*(\\d+),\"([^\"]*)\",\"([^\"]*)\",[^,]*,\"([^\"]*)\"");
    QRegularExpressionMatchIterator it = re.globalMatch(resp);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        SmsEntry entry;
        entry.index     = match.captured(1).toInt();
        entry.stat      = match.captured(2);
        entry.sender    = match.captured(3);
        entry.timestamp = match.captured(4);
        entries.append(entry);
    }

    // 解析每条消息的正文（在 +CMGL 和 OK 之间）
    QStringList lines = resp.split(QRegularExpression("\\r\\n|\\n"));
    for (SmsEntry &entry : entries) {
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains(QString("+CMGL: %1,").arg(entry.index))) {
                if (i + 1 < lines.size() && !lines[i + 1].startsWith("+CMGL:") && lines[i + 1] != "OK") {
                    entry.content = lines[i + 1].trimmed();
                }
                break;
            }
        }
    }

    qDebug() << "SMS list:" << entries.size() << "messages";
    return entries;
}

// ==================== GPS / GNSS 定位 ====================

bool EC20Manager::startGps(const QString &nmeaPort)
{
    // 第一步：配置 NMEA 输出端口
    // "usbnmea" → ttyUSB1 端口（独立 NMEA 数据流）
    // "none"    → 仅通过 AT+QGPSLOC 查询，不开 NMEA 输出
    QString outport = nmeaPort.isEmpty() ? "usbnmea" : nmeaPort;
    QString resp    = sendAtCommand(QString("AT+QGPSCFG=\"outport\",\"%1\"").arg(outport));
    if (!resp.contains("OK"))
        return false;

    // 第二步：配置 GNSS 星座（1=GPS, 2=GLONASS, 3=GPS+GLONASS）
    sendAtCommand("AT+QGPSCFG=\"gnssconfig\",3");

    // 第三步：开启 GNSS
    resp = sendAtCommand("AT+QGPS=1");
    if (!resp.contains("OK"))
        return false;

    // 第四步：如果使用独立 NMEA 端口，打开它
    if (outport == "usbnmea") {
        QString port = nmeaPort;
        if (port.isEmpty() || port == "usbnmea") {
            const auto ports = QSerialPortInfo::availablePorts();
            for (const auto &info : ports) {
                if (info.portName().contains("ttyUSB1")) {
                    port = info.portName();
                    break;
                }
            }
        }
        if (!port.isEmpty() && port != m_atPort) {
            openNmeaPort(port);
        } else {
            qDebug() << "GPS: NMEA port ttyUSB1 not found, falling back to AT+QGPSLOC polling";
        }
    }

    m_gpsStarted = true;
    qDebug() << "GPS started, NMEA outport:" << outport;
    return true;
}

bool EC20Manager::stopGps()
{
    sendAtCommand("AT+QGPSEND");
    closeNmeaPort();
    m_gpsFixed  = false;
    m_gpsStarted = false;
    emit gpsStatusChanged(false);
    return true;
}

bool EC20Manager::setNmeaOutput(bool enable)
{
    // 启用/禁用通过 AT+QGPSGNMEA 在 AT 端口获取 NMEA 语句
    QString cmd  = QString("AT+QGPSCFG=\"nmeasrc\",%1").arg(enable ? 1 : 0);
    QString resp = sendAtCommand(cmd);
    if (resp.contains("OK")) {
        return true;
    }
    return false;
}

bool EC20Manager::openNmeaPort(const QString &port)
{
    if (m_nmeaSerial.isOpen()) {
        if (m_nmeaSerial.portName() == port)
            return true;
        m_nmeaSerial.close();
    }

    m_nmeaSerial.setPortName(port);
    m_nmeaSerial.setBaudRate(QSerialPort::Baud115200);
    m_nmeaSerial.setDataBits(QSerialPort::Data8);
    m_nmeaSerial.setParity(QSerialPort::NoParity);
    m_nmeaSerial.setStopBits(QSerialPort::OneStop);
    m_nmeaSerial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_nmeaSerial.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Failed to open NMEA port %1: %2").arg(port, m_nmeaSerial.errorString()));
        return false;
    }

    connect(&m_nmeaSerial, &QSerialPort::readyRead, this, &EC20Manager::onNmeaReadyRead);
    qDebug() << "NMEA port opened:" << port;
    return true;
}

void EC20Manager::closeNmeaPort()
{
    if (m_nmeaSerial.isOpen()) {
        disconnect(&m_nmeaSerial, &QSerialPort::readyRead, this, &EC20Manager::onNmeaReadyRead);
        m_nmeaSerial.close();
    }
}

void EC20Manager::onNmeaReadyRead()
{
    QByteArray data   = m_nmeaSerial.readAll();
    QString str       = QString::fromUtf8(data);
    QStringList lines = str.split('\n');
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (trimmed.startsWith('$')) {
            emit newNmeaSentence(trimmed);
            parseNmea(trimmed);
        }
    }
}

bool EC20Manager::queryLocationOnce(double &longitude, double &latitude)
{
    // 确保 GPS 已开启
    QString statusResp = sendAtCommand("AT+QGPS?");
    if (statusResp.contains("+QGPS: 0")) {
        qDebug() << "GPS not started, attempting to start...";
        if (!startGps()) {
            emit errorOccurred("GPS not running and cannot be started");
            return false;
        }
    }

    QString resp = sendAtCommand("AT+QGPSLOC=2", 5000);
    if (resp.isEmpty())
        return false;
    return parseQGPSLOC(resp, longitude, latitude);
}

bool EC20Manager::parseQGPSLOC(const QString &response, double &lon, double &lat)
{
    // 示例：+QGPSLOC: 061951.0,3150.7223N,11711.9293E,0.7,62.2,2,0.0,0.0,0.0,110513,09
    QRegularExpression re("\\+QGPSLOC:\\s*(\\d+\\.\\d+),\\s*([0-9.]+)([NS]),\\s*([0-9.]+)([EW])");
    QRegularExpressionMatch match = re.match(response);
    if (match.hasMatch()) {
        double rawLat = match.captured(2).toDouble();
        double rawLon = match.captured(4).toDouble();
        lat           = convertToDecimalDegrees(QString::number(rawLat), true);
        lon           = convertToDecimalDegrees(QString::number(rawLon), false);
        if (match.captured(3) == "S") lat = -lat;
        if (match.captured(5) == "W") lon = -lon;

        emit positionUpdated(lon, lat);
        return true;
    }
    return false;
}

double EC20Manager::convertToDecimalDegrees(const QString &nmeaCoord, bool isLat)
{
    Q_UNUSED(isLat);
    double coord   = nmeaCoord.toDouble();
    int degrees    = static_cast<int>(coord / 100);
    double minutes = coord - (degrees * 100);
    return degrees + minutes / 60.0;
}

void EC20Manager::parseNmea(const QString &sentence)
{
    if (sentence.startsWith("$GPRMC") || sentence.startsWith("$GNRMC")) {
        QStringList fields = sentence.split(',');
        if (fields.size() >= 7 && fields[2] == "A") {
            double lat = convertToDecimalDegrees(fields[3], true);
            double lon = convertToDecimalDegrees(fields[5], false);
            if (fields[4] == "S") lat = -lat;
            if (fields[6] == "W") lon = -lon;

            if (!m_gpsFixed) {
                m_gpsFixed = true;
                emit gpsStatusChanged(true);
            }
            emit positionUpdated(lon, lat);
        } else if (m_gpsFixed) {
            m_gpsFixed = false;
            emit gpsStatusChanged(false);
        }
    } else if (sentence.startsWith("$GPGGA") || sentence.startsWith("$GNGGA")) {
        QStringList fields = sentence.split(',');
        if (fields.size() >= 7 && !fields[2].isEmpty() && !fields[4].isEmpty()) {
            int quality = fields[6].toInt();
            if (quality > 0) {
                double lat = convertToDecimalDegrees(fields[2], true);
                double lon = convertToDecimalDegrees(fields[4], false);
                if (fields[3] == "S") lat = -lat;
                if (fields[5] == "W") lon = -lon;

                if (!m_gpsFixed) {
                    m_gpsFixed = true;
                    emit gpsStatusChanged(true);
                }
                emit positionUpdated(lon, lat);
            }
        }
    }
}

// ==================== GPS Mock 回放（室内调试用） ====================

bool EC20Manager::loadNmeaFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法打开 NMEA 文件: %1").arg(path));
        return false;
    }

    m_mockNmeaLines.clear();
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith('$'))
            m_mockNmeaLines.append(line);
    }
    file.close();

    if (m_mockNmeaLines.isEmpty()) {
        emit errorOccurred("NMEA 文件中没有有效语句");
        return false;
    }

    qDebug() << "Loaded" << m_mockNmeaLines.size() << "NMEA sentences from" << path;
    return true;
}

bool EC20Manager::startMockGps(int intervalMs)
{
    if (m_mockNmeaLines.isEmpty()) {
        emit errorOccurred("没有加载 NMEA 数据，请先调用 loadNmeaFile()");
        return false;
    }

    if (isMockActive())
        stopMockGps();

    stopGps();

    m_mockNmeaIndex = 0;
    m_mockTimer     = new QTimer(this);
    connect(m_mockTimer, &QTimer::timeout, this, &EC20Manager::onMockTimerTick);
    m_mockTimer->start(intervalMs);

    if (!m_gpsFixed) {
        m_gpsFixed = true;
        emit gpsStatusChanged(true);
    }

    qDebug() << "Mock GPS started, interval:" << intervalMs << "ms,"
             << m_mockNmeaLines.size() << "sentences";
    return true;
}

void EC20Manager::stopMockGps()
{
    if (m_mockTimer) {
        m_mockTimer->stop();
        delete m_mockTimer;
        m_mockTimer = nullptr;
    }
    m_mockNmeaIndex = 0;
    if (m_gpsFixed) {
        m_gpsFixed = false;
        emit gpsStatusChanged(false);
    }
    qDebug() << "Mock GPS stopped";
}

void EC20Manager::onMockTimerTick()
{
    if (m_mockNmeaIndex >= m_mockNmeaLines.size())
        m_mockNmeaIndex = 0;

    const QString &sentence = m_mockNmeaLines[m_mockNmeaIndex++];
    emit newNmeaSentence(sentence);
    parseNmea(sentence);
}

// ==================== AT 指令底层通信 ====================

QString EC20Manager::sendAtCommand(const QString &cmd, int timeoutMs)
{
    QString response;
    if (sendAtCommand(cmd, response, timeoutMs))
        return response;
    return QString();
}

bool EC20Manager::sendAtCommand(const QString &cmd, QString &response, int timeoutMs)
{
    if (!m_serial.isOpen()) {
        emit errorOccurred("Serial port not open");
        return false;
    }

    // 进入同步命令模式，抑制 onReadyRead 干扰
    m_inCommand = true;
    QMutexLocker locker(&m_serialMutex);

    m_serial.write((cmd + "\r\n").toUtf8());
    if (!m_serial.waitForBytesWritten(500)) {
        emit errorOccurred("Write timeout for command: " + cmd);
        m_inCommand = false;
        return false;
    }

    QByteArray allData;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (m_serial.waitForReadyRead(200)) {
            allData += m_serial.readAll();
            // 检查是否收到完整响应
            if (allData.contains("OK\r\n") || allData.contains("ERROR\r\n") ||
                allData.contains("> ")) { // SMS 输入提示
                break;
            }
        }
    }

    if (allData.isEmpty()) {
        emit errorOccurred("Read timeout for command: " + cmd);
        m_inCommand = false;
        return false;
    }

    response    = QString::fromUtf8(allData).trimmed();
    m_inCommand = false;
    return true;
}

void EC20Manager::onReadyRead()
{
    // 同步命令执行中不处理异步数据，避免竞态
    if (m_inCommand)
        return;

    m_buffer.append(m_serial.readAll());

    int idx;
    while ((idx = m_buffer.indexOf('\n')) != -1) {
        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        QString str = QString::fromUtf8(line);
        if (str.isEmpty())
            continue;

        // 过滤 AT 回显和 OK/ERROR（同步命令期间已处理）
        if (str == "OK" || str == "ERROR")
            continue;

        if (str.startsWith('$')) {
            emit newNmeaSentence(str);
            parseNmea(str);
        } else if (str.startsWith("+CMTI:")) {
            QRegularExpression re("\\+CMTI:\\s*\"[^\"]+\",(\\d+)");
            QRegularExpressionMatch match = re.match(str);
            if (match.hasMatch()) {
                int index = match.captured(1).toInt();
                emit smsReceived(index);
            }
        } else if (str.startsWith("+CSQ:")) {
            // 信号质量 URC（如果开启了自动上报）
            QRegularExpression re("\\+CSQ:\\s*(\\d+),\\s*(\\d+)");
            QRegularExpressionMatch match = re.match(str);
            if (match.hasMatch()) {
                emit signalQualityUpdated(match.captured(1).toInt(), match.captured(2).toInt());
            }
        } else if (str.startsWith("+CREG:")) {
            QRegularExpression re("\\+CREG:\\s*\\d+,\\s*(\\d+)");
            QRegularExpressionMatch match = re.match(str);
            if (match.hasMatch()) {
                emit networkRegistrationChanged(match.captured(1).toInt());
            }
        }
    }
}
