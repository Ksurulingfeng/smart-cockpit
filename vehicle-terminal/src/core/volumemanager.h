#ifndef VOLUMEMANAGER_H
#define VOLUMEMANAGER_H

#include <QObject>
#include <QMediaPlayer>
#include <QPointer>
#include <QVector>

class VolumeManager : public QObject
{
    Q_OBJECT
public:
    // 禁止拷贝和赋值
    VolumeManager(const VolumeManager &)            = delete;
    VolumeManager &operator=(const VolumeManager &) = delete;

    // 全局访问点（线程安全）
    static VolumeManager *instance();

    // 注册/注销播放器（每个播放器创建时注册，销毁时自动注销）
    void registerPlayer(QMediaPlayer *player);
    void unregisterPlayer(QMediaPlayer *player);

    // 设置音量 (0-100)
    void setVolume(int volume);

    // 获取当前音量
    int volume() const
    {
        return m_volume;
    }

signals:
    void volumeChanged(int volume);

private:
    explicit VolumeManager(QObject *parent = nullptr);
    ~VolumeManager() override = default;

    QVector<QPointer<QMediaPlayer>> m_players;
    int m_volume = 50; // 默认音量 50%
};

#endif // VOLUMEMANAGER_H