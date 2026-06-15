#ifndef MUSIC_PAGE_H
#define MUSIC_PAGE_H

#include <QWidget>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QTimer>
#include <QListWidget>
#include <QPushButton>
#include <QVector>

namespace Ui
{
    class MusicPage;
}

struct SongInfo {
    QString fileName;  // 原始文件名
    QString title;     // 歌名
    QString singer;    // 歌手
    QString filePath;  // 完整路径
    QString lyricPath; // 歌词文件路径
    QString coverPath; // 封面图片路径
};

class MusicPage : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPage(QWidget *parent = nullptr);
    ~MusicPage() override;

    bool isPlaying() const
    {
        return m_player->state() == QMediaPlayer::PlayingState;
    }
    SongInfo getCurrentSongInfo() const
    {
        int idx = m_playlist->currentIndex();
        return (idx >= 0 && idx < m_songList.size()) ? m_songList[idx] : SongInfo();
    }

public slots:
    void onPlayPause(); // 播放/暂停
    void onPrevious();  // 上一首
    void onNext();      // 下一首

protected:
signals:
    void stateChanged(bool isPlaying); // 播放状态改变信号
    void mediaChanged(SongInfo info);  // 当前媒体改变信号

private slots:
    void onProgressSliderMoved(int value);          // 进度条拖动
    void onPlayModeClicked();                       // 播放模式切换
    void onPlayListClicked();                       // 播放列表点击
    void onStateChanged(QMediaPlayer::State state); // 播放状态改变
    void onDurationChanged(qint64 duration);        // 歌曲时长改变
    void onPositionChanged(qint64 position);        // 播放位置改变
    void onCurrentMediaChanged(int index);          // 当前媒体改变
    void onLyricItemClicked(QListWidgetItem *item); // 歌词列表点击

private:
    void setupPlayer();                         // 初始化媒体播放器
    void setupConfig();                         // 加载配置
    void setPlayMode(int mode);                 // 设置播放模式
    void scanSongs();                           // 扫描歌曲
    void loadLyrics(const QString &lyricPath);  // 加载歌词
    void updateLyric(qint64 position);          // 更新歌词
    void updateCover(const QString &coverPath); // 更新封面
    void updateSongInfo(int index);             // 更新歌曲信息
    void setupIcons();                          // 初始化图标

    Ui::MusicPage *ui;
    QMediaPlayer *m_player;       // 媒体播放器
    QMediaPlaylist *m_playlist;   // 播放列表
    QVector<SongInfo> m_songList; // 歌曲列表

    struct LyricItem {
        qint64 time;  // 毫秒
        QString text; // 歌词文本
    }; // 歌词解析QVector
    QVector<LyricItem> m_lyricLines; // 歌词列表
    int m_currentLyricIndex;         // 当前歌词索引

    QMediaPlaylist::PlaybackMode m_playMode; // 播放模式
};

#endif // MUSIC_PAGE_H