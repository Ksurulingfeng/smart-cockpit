#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QVariant>
#include <QSettings>

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    static ConfigManager *instance();

    ConfigManager(const ConfigManager &)            = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;

    // 读写通用接口
    void setValue(const QString &key, const QVariant &value);
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;

    // 便捷方法
    int volume() const;
    void setVolume(int volume);
    int playMode() const;
    void setPlayMode(int mode);
    QString currentSongPath() const;
    void setCurrentSongPath(const QString &path);
signals:
    void volumeChanged(int volume);
    void playModeChanged(int mode);

private:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() override;

    QSettings *m_settings;
};

#endif // CONFIGMANAGER_H