#include "sysfsled.h"
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>

SysfsLed::SysfsLed(const QString &path, QObject *parent)
    : QObject(parent)
    , m_brightnessFile(path)
    , m_triggerFile(QFileInfo(path).path() + "/trigger")
    , m_valid(false)
{
    if (m_brightnessFile.open(QIODevice::ReadWrite)) {
        m_valid = true;
    } else {
        qWarning() << "Failed to open brightness file:" << path;
        emit errorOccurred("Cannot open brightness file");
    }
    // trigger 文件可选，打开失败不影响基本功能
    if (!m_triggerFile.open(QIODevice::ReadWrite)) {
        qWarning() << "Failed to open trigger file:" << m_triggerFile.fileName();
    }
}

SysfsLed::~SysfsLed()
{
    if (m_brightnessFile.isOpen())
        m_brightnessFile.close();
    if (m_triggerFile.isOpen())
        m_triggerFile.close();
}

void SysfsLed::setState(bool on)
{
    if (!m_valid) return;
    m_brightnessFile.seek(0);
    QTextStream out(&m_brightnessFile);
    out << (on ? "1" : "0");
    m_brightnessFile.flush();
}

bool SysfsLed::state() const
{
    if (!m_valid) return false;
    const_cast<QFile &>(m_brightnessFile).seek(0);
    QTextStream in(&const_cast<QFile &>(m_brightnessFile));
    QString val = in.readAll().trimmed();
    return (val == "1");
}

void SysfsLed::setTrigger(const QString &trigger)
{
    if (!m_triggerFile.isOpen()) return;
    m_triggerFile.seek(0);
    QTextStream out(&m_triggerFile);
    out << trigger;
    m_triggerFile.flush();
}

QString SysfsLed::trigger() const
{
    if (!m_triggerFile.isOpen()) return QString();
    const_cast<QFile &>(m_triggerFile).seek(0);
    QTextStream in(&const_cast<QFile &>(m_triggerFile));
    return in.readAll().trimmed();
}