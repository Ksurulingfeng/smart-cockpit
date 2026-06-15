#include "configmanager.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

ConfigManager *ConfigManager::instance()
{
    static ConfigManager instance;
    return &instance;
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    // 存储路径：可执行文件同目录下的 config.ini
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    m_settings         = new QSettings(configPath, QSettings::IniFormat, this);
    qDebug() << "Config file:" << configPath;
}

ConfigManager::~ConfigManager()
{
}

void ConfigManager::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
    m_settings->sync(); // 立即写入磁盘
}

QVariant ConfigManager::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

// ---------- 音量 ----------
int ConfigManager::volume() const
{
    return value("audio/volume", 50).toInt();
}

void ConfigManager::setVolume(int volume)
{
    volume = qBound(0, volume, 100);
    setValue("audio/volume", volume);
    emit volumeChanged(volume);
}

// ---------- 播放模式 ----------
int ConfigManager::playMode() const
{
    return value("music/playMode", 0).toInt();
}

void ConfigManager::setPlayMode(int mode)
{
    setValue("music/playMode", mode);
    emit playModeChanged(mode);
}

// ---------- 当前播放歌曲 ----------
QString ConfigManager::currentSongPath() const
{
    return value("music/currentSong", "").toString();
}

void ConfigManager::setCurrentSongPath(const QString &path)
{
    setValue("music/currentSong", path);
}