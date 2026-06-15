#include "backlightcontrol.h"
#include <QTextStream>
#include <QDir>
#include <QDebug>

BacklightControl::BacklightControl(const QString &sysfsPath, QObject *parent)
    : QObject(parent)
    , m_valid(false)
{
    QString brightnessPath = findBrightnessPath(sysfsPath);
    if (brightnessPath.isEmpty()) {
        qWarning() << "Backlight not found at" << sysfsPath;
        emit errorOccurred("Backlight not found");
        return;
    }

    m_maxFile.setFileName(brightnessPath + "/max_brightness");
    m_currFile.setFileName(brightnessPath + "/actual_brightness");
    m_brightnessFile.setFileName(brightnessPath + "/brightness");

    if (m_maxFile.open(QIODevice::ReadOnly) &&
        m_currFile.open(QIODevice::ReadOnly) &&
        m_brightnessFile.open(QIODevice::ReadWrite)) {
        m_valid = true;
    } else {
        qWarning() << "Failed to open backlight files";
        emit errorOccurred("Cannot open backlight sysfs");
    }
}

BacklightControl::~BacklightControl()
{
    if (m_maxFile.isOpen()) m_maxFile.close();
    if (m_currFile.isOpen()) m_currFile.close();
    if (m_brightnessFile.isOpen()) m_brightnessFile.close();
}

QString BacklightControl::findBrightnessPath(const QString &basePath)
{
    QDir dir(basePath);
    if (!dir.exists()) return QString();

    // 尝试直接使用 basePath + "/brightness"
    if (QFile::exists(basePath + "/brightness"))
        return basePath;

    // 否则查找子目录，例如 backlight 下可能还有子目录
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &sub : subdirs) {
        QString candidate = basePath + "/" + sub;
        if (QFile::exists(candidate + "/brightness"))
            return candidate;
    }
    return QString();
}

int BacklightControl::maxBrightness() const
{
    if (!m_valid) return -1;
    const_cast<QFile &>(m_maxFile).seek(0);
    QTextStream in(&const_cast<QFile &>(m_maxFile));
    return in.readAll().trimmed().toInt();
}

int BacklightControl::currentBrightness() const
{
    if (!m_valid) return -1;
    const_cast<QFile &>(m_currFile).seek(0);
    QTextStream in(&const_cast<QFile &>(m_currFile));
    return in.readAll().trimmed().toInt();
}

bool BacklightControl::setBrightness(int value)
{
    if (!m_valid) return false;
    int max = maxBrightness();
    if (max <= 0) return false;
    value = qBound(0, value, max);
    m_brightnessFile.seek(0);
    QTextStream out(&m_brightnessFile);
    out << value;
    m_brightnessFile.flush();
    return true;
}

bool BacklightControl::setBrightnessPercent(int percent)
{
    int max = maxBrightness();
    if (max <= 0) return false;
    int target = (percent * max) / 100;
    return setBrightness(target);
}