#include "wifimanager.h"
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

// connmanctl services 输出行格式：前缀(3字符) + SSID + service路径
static const QRegularExpression RE_SERVICE(R"(^([*AORac ]{3})\s+(.+?)\s+(\S+)$)");

// connmanctl technologies 中 WiFi 段的 Powered 字段
static const QRegularExpression RE_WIFI_POWERED(R"(Type\s*=\s*wifi[\s\S]*?Powered\s*=\s*(True|False))",
                                                QRegularExpression::MultilineOption);
static const QRegularExpression RE_WIFI_TYPE(R"(Type\s*=\s*wifi)");

WifiManager *WifiManager::instance()
{
    static WifiManager inst;
    return &inst;
}

WifiManager::WifiManager(QObject *parent)
    : QObject(parent)
{
    QTimer::singleShot(0, this, &WifiManager::init);
}

WifiManager::~WifiManager() = default;

// ==================== 初始化 ====================

void WifiManager::init()
{
    queryTechnologies();
    // 异步获取已连接的网络信息
    runCommand({"services"}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &output) {
        if (exitCode == 0 && status == QProcess::NormalExit)
            updateConnectionState(parseServicesOutput(QString::fromUtf8(output)));
    });
}

// ==================== 缓存的技术查询 ====================

void WifiManager::queryTechnologies()
{
    QProcess proc;
    proc.start("connmanctl", {"technologies"});
    if (!proc.waitForFinished(2000))
        return;
    QString out     = proc.readAllStandardOutput();
    m_techQueried   = true;
    m_wifiAvailable = RE_WIFI_TYPE.match(out).hasMatch();
    m_wifiPowered   = RE_WIFI_POWERED.match(out).captured(1) == "True";
}

bool WifiManager::isWifiEnabled() const
{
    if (!m_techQueried)
        const_cast<WifiManager *>(this)->queryTechnologies();
    return m_wifiPowered;
}

bool WifiManager::isWifiAvailable() const
{
    if (!m_techQueried)
        const_cast<WifiManager *>(this)->queryTechnologies();
    return m_wifiAvailable;
}

// ==================== 公共接口 ====================

void WifiManager::enableWifi()
{
    runCommand({"enable", "wifi"}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            m_wifiPowered = true;
            emit enabledWifi();
        } else {
            emit errorOccurred("启用 WiFi 失败");
        }
    });
}

void WifiManager::disableWifi()
{
    runCommand({"disable", "wifi"}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            m_wifiPowered = false;
            emit disabledWifi();
        } else {
            emit errorOccurred("禁用 WiFi 失败");
        }
    });
}

void WifiManager::scanNetworks()
{
    runCommand({"scan", "wifi"}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &) {
        if (exitCode != 0 || status != QProcess::NormalExit) {
            emit errorOccurred("扫描失败");
            return;
        }
        runCommand({"services"}, [this](int code, QProcess::ExitStatus stat, const QByteArray &out) {
            if (code != 0 || stat != QProcess::NormalExit) {
                emit errorOccurred("获取服务列表失败");
                return;
            }
            auto nets = parseServicesOutput(QString::fromUtf8(out));
            updateConnectionState(nets);
            emit scanFinished(nets);
        });
    });
}

void WifiManager::connectNetwork(const QString &ssid, const QString &password)
{
    m_connectRetryCount = 0;
    m_pendingSsid       = ssid;

    getServicePathAsync(ssid, [this, ssid, password](const QString &path) {
        if (path.isEmpty()) {
            emit connectionFailed(ssid, "未找到该网络服务");
            return;
        }
        if (!password.isEmpty() && !writeWifiConfig(path, ssid, password)) {
            emit connectionFailed(ssid, "写入配置文件失败");
            return;
        }
        runCommand({"connect", path}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &) {
            if (exitCode != 0 || status != QProcess::NormalExit) {
                emit connectionFailed(m_pendingSsid, "连接命令失败");
                m_pendingSsid.clear();
                return;
            }
            QTimer::singleShot(0, this, &WifiManager::checkConnectionStatus);
        });
    });
}

void WifiManager::disconnect()
{
    getCurrentServicePathAsync([this](const QString &path) {
        if (path.isEmpty()) {
            emit disconnected();
            return;
        }
        runCommand({"disconnect", path}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &) {
            if (exitCode == 0 && status == QProcess::NormalExit) {
                m_connected = false;
                m_currentSsid.clear();
                emit disconnected();
            } else {
                emit errorOccurred("断开连接失败");
            }
        });
    });
}

void WifiManager::forgetNetwork(const QString &ssid)
{
    getServicePathAsync(ssid, [this, ssid](const QString &path) {
        if (path.isEmpty()) {
            emit networkForgotten(ssid);
            return;
        }
        // 如果当前正连接着这个网络，先断开再删除
        auto remove = [this, path, ssid]() {
            runCommand({"config", path, "--remove"}, [this, path, ssid](int code, QProcess::ExitStatus status, const QByteArray &) {
                removeWifiConfig(path);
                emit(code == 0 && status == QProcess::NormalExit ? networkForgotten(ssid)
                                                                 : errorOccurred("忘记网络失败"));
            });
        };
        getCurrentServicePathAsync([this, path, remove](const QString &current) {
            if (current == path)
                runCommand({"disconnect", path}, [remove](int, QProcess::ExitStatus, const QByteArray &) { remove(); });
            else
                remove();
        });
    });
}

// ==================== 辅助函数 ====================

QVector<WifiNetwork> WifiManager::parseServicesOutput(const QString &output)
{
    QVector<WifiNetwork> nets;
    for (const QString &line : output.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        auto match = RE_SERVICE.match(line);
        if (!match.hasMatch()) {
            qDebug() << "Unmatched service line:" << line;
            continue;
        }
        WifiNetwork net;
        QString prefix  = match.captured(1);
        net.favorite    = prefix.contains('*');
        net.autoConnect = prefix.contains('A');
        net.connected   = prefix.contains('R') || prefix.contains('O');
        net.ssid        = match.captured(2).trimmed();
        net.service     = match.captured(3).trimmed();
        net.secured     = net.service.contains("_psk") || net.service.contains("_ieee8021x");
        nets.append(net);
    }
    return nets;
}

void WifiManager::updateConnectionState(const QVector<WifiNetwork> &nets)
{
    bool wasConnected   = m_connected;
    QString oldSsid     = m_currentSsid;

    m_connected = false;
    m_currentSsid.clear();
    for (const auto &net : nets) {
        if (net.connected) {
            m_connected   = true;
            m_currentSsid = net.ssid;
            break;
        }
    }

    if (m_connected && !wasConnected)
        emit connectionSuccess(m_currentSsid);
    else if (!m_connected && wasConnected)
        emit disconnected();
}

void WifiManager::getServicePathAsync(const QString &ssid, std::function<void(const QString &)> callback)
{
    runCommand({"services"}, [this, ssid, cb = std::move(callback)](int code, QProcess::ExitStatus status, const QByteArray &output) {
        if (code != 0 || status != QProcess::NormalExit) {
            if (cb) cb(QString());
            return;
        }
        for (const auto &net : parseServicesOutput(QString::fromUtf8(output))) {
            if (net.ssid == ssid) {
                if (cb) cb(net.service);
                return;
            }
        }
        if (cb) cb(QString());
    });
}

void WifiManager::getCurrentServicePathAsync(std::function<void(const QString &)> callback)
{
    runCommand({"services"}, [this, cb = std::move(callback)](int code, QProcess::ExitStatus status, const QByteArray &output) {
        if (code != 0 || status != QProcess::NormalExit) {
            if (cb) cb(QString());
            return;
        }
        for (const auto &net : parseServicesOutput(QString::fromUtf8(output))) {
            if (net.connected) {
                if (cb) cb(net.service);
                return;
            }
        }
        if (cb) cb(QString());
    });
}

bool WifiManager::writeWifiConfig(const QString &servicePath, const QString &ssid, const QString &password)
{
    QString configPath = "/var/lib/connman/" + servicePath + ".config";
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot write config:" << configPath;
        return false;
    }
    QTextStream out(&file);
    out << "[service_" << servicePath << "]\n"
        << "Type = wifi\n"
        << "Name = " << ssid << "\n"
        << "Passphrase = " << password << "\n";
    file.close();
    return true;
}

void WifiManager::removeWifiConfig(const QString &servicePath)
{
    if (!servicePath.isEmpty())
        QFile::remove("/var/lib/connman/" + servicePath + ".config");
}

void WifiManager::checkConnectionStatus()
{
    if (++m_connectRetryCount > 10) {
        emit connectionFailed(m_pendingSsid, "连接超时");
        m_pendingSsid.clear();
        return;
    }

    runCommand({"services"}, [this](int exitCode, QProcess::ExitStatus status, const QByteArray &output) {
        if (exitCode != 0 || status != QProcess::NormalExit) {
            QTimer::singleShot(500, this, &WifiManager::checkConnectionStatus);
            return;
        }
        for (const auto &net : parseServicesOutput(QString::fromUtf8(output))) {
            if (net.ssid != m_pendingSsid)
                continue;
            if (net.connected) {
                m_connected   = true;
                m_currentSsid = net.ssid;
                emit connectionSuccess(m_pendingSsid);
                m_pendingSsid.clear();
                return;
            }
            // 找到了但未连接 → 继续等待
            QTimer::singleShot(500, this, &WifiManager::checkConnectionStatus);
            return;
        }
        // 服务列表中找不到这个 SSID → 认证失败
        emit connectionFailed(m_pendingSsid, "认证失败（密码错误？）");
        m_pendingSsid.clear();
    });
}

// ==================== 命令队列 ====================

void WifiManager::runCommand(const QStringList &args,
                             std::function<void(int, QProcess::ExitStatus, const QByteArray &)> callback)
{
    if (m_commandRunning) {
        m_commandQueue.enqueue([this, args, callback]() { runCommand(args, callback); });
        return;
    }

    m_commandRunning = true;
    auto *proc       = new QProcess(this);

    auto finish      = [this, proc, cb = std::move(callback)]() {
        if (cb) cb(proc->exitCode(), proc->exitStatus(), proc->readAllStandardOutput());
        proc->deleteLater();
        m_commandRunning = false;
        dequeueNext();
    };

    connect(proc, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError) { finish(); });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [finish](int, QProcess::ExitStatus) { finish(); });

    proc->start("connmanctl", args);
}

void WifiManager::dequeueNext()
{
    if (!m_commandQueue.isEmpty()) {
        auto next = m_commandQueue.dequeue();
        next();
    }
}
