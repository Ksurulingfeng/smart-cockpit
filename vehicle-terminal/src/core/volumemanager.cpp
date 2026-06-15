#include "volumemanager.h"

VolumeManager *VolumeManager::instance()
{
    static VolumeManager instance;
    return &instance;
}

VolumeManager::VolumeManager(QObject *parent)
    : QObject(parent)
{
}

void VolumeManager::registerPlayer(QMediaPlayer *player)
{
    if (!player)
        return;
    if (!m_players.contains(player)) {
        m_players.append(player);
        // 同步当前音量
        player->setVolume(m_volume);
    }
}

void VolumeManager::unregisterPlayer(QMediaPlayer *player)
{
    m_players.removeAll(player);
}

void VolumeManager::setVolume(int volume)
{
    m_volume = qBound(0, volume, 100);
    for (auto &player : m_players) {
        if (player) {
            player->setVolume(m_volume);
        }
    }
    emit volumeChanged(m_volume);
}