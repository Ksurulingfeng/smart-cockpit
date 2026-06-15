#include "musicPage.h"
#include "ui_musicPage.h"
#include "volumemanager.h"
#include "config.h"
#include "configmanager.h"
#include "iconhelper.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTime>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QListWidget>
#include <QScroller>
#include <QScrollBar>
#include <QSettings>

MusicPage::MusicPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MusicPage)
    , m_player(nullptr)
    , m_playlist(nullptr)
    , m_currentLyricIndex(-1)
    , m_playMode(QMediaPlaylist::PlaybackMode::Loop)
{
    ui->setupUi(this);

    // 歌词列表：触摸滚动 + 隐藏侧边滑块
    ui->listWidget_lyric->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listWidget_lyric->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listWidget_lyric->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    QScroller::grabGesture(ui->listWidget_lyric->viewport(), QScroller::LeftMouseButtonGesture);
    QScrollerProperties sp;
    sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0);
    QScroller::scroller(ui->listWidget_lyric->viewport())->setScrollerProperties(sp);

    // 进度条样式
    ui->slider_progress->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  height: 4px;"
        "  background: #d0d0d0;"
        "  border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #1db954;"
        "  border-radius: 2px;"
        "}"
        "QSlider::add-page:horizontal {"
        "  background: rgba(0,0,0,0);"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #1db954;"
        "  width: 14px;"
        "  height: 14px;"
        "  margin: -5px 0;"
        "  border-radius: 7px;"
        "}");

    setupPlayer();
    setupIcons();
    scanSongs();
    setupConfig();

    // 信号连接
    connect(ui->toolButton_play, &QToolButton::clicked, this, &MusicPage::onPlayPause);
    connect(ui->toolButton_previous, &QToolButton::clicked, this, &MusicPage::onPrevious);
    connect(ui->toolButton_next, &QToolButton::clicked, this, &MusicPage::onNext);
    connect(ui->toolButton_playMode, &QToolButton::clicked, this, &MusicPage::onPlayModeClicked);
    connect(ui->toolButton_playList, &QToolButton::clicked, this, &MusicPage::onPlayListClicked);
    connect(ui->slider_progress, &MySlider::sliderMoved, this, &MusicPage::onProgressSliderMoved);
}

MusicPage::~MusicPage()
{
    VolumeManager::instance()->unregisterPlayer(m_player);
    delete ui;
}

void MusicPage::setupPlayer()
{
    m_player   = new QMediaPlayer(this);
    m_playlist = new QMediaPlaylist(this);
    m_player->setPlaylist(m_playlist);
    VolumeManager::instance()->registerPlayer(m_player);

    // 连接信号
    connect(m_player, &QMediaPlayer::stateChanged, this, &MusicPage::onStateChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MusicPage::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MusicPage::onPositionChanged);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, &MusicPage::onCurrentMediaChanged);
    connect(ui->listWidget_lyric, &QListWidget::itemClicked, this, &MusicPage::onLyricItemClicked);
}

void MusicPage::setupConfig()
{
    // 加载配置
    int savedVolume = ConfigManager::instance()->volume();
    VolumeManager::instance()->setVolume(savedVolume);
    int savedMode = ConfigManager::instance()->playMode();
    setPlayMode(savedMode);
}

void MusicPage::setPlayMode(int mode)
{
    switch (mode) {
        case 0: // 顺序播放
            m_playMode = QMediaPlaylist::Sequential;
            m_playlist->setPlaybackMode(QMediaPlaylist::Sequential);
            ui->toolButton_playMode->setIcon(QIcon(":/images/images/顺序播放.png"));
            break;
        case 1: // 列表循环
            m_playMode = QMediaPlaylist::Loop;
            m_playlist->setPlaybackMode(QMediaPlaylist::Loop);
            ui->toolButton_playMode->setIcon(QIcon(":/images/images/列表循环.png"));
            break;
        case 2: // 单曲循环
            m_playMode = QMediaPlaylist::CurrentItemInLoop;
            m_playlist->setPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
            ui->toolButton_playMode->setIcon(QIcon(":/images/images/单曲循环.png"));
            break;
        case 3: // 随机播放
            m_playMode = QMediaPlaylist::Random;
            m_playlist->setPlaybackMode(QMediaPlaylist::Random);
            ui->toolButton_playMode->setIcon(QIcon(":/images/images/随机播放.png"));
            break;
        default:
            m_playMode = QMediaPlaylist::Loop;
            m_playlist->setPlaybackMode(QMediaPlaylist::Loop);
            ui->toolButton_playMode->setIcon(QIcon(":/images/images/列表循环.png"));
            break;
    }
    IconHelper::setupButton(ui->toolButton_playMode, Qt::black);
}

void MusicPage::setupIcons()
{
    IconHelper::setupButton(ui->toolButton_previous, Qt::black);
    IconHelper::setupButton(ui->toolButton_play, Qt::black);
    IconHelper::setupButton(ui->toolButton_next, Qt::black);
    IconHelper::setupButton(ui->toolButton_playList, Qt::black);
    IconHelper::setupButton(ui->toolButton_playMode, Qt::black);
}

void MusicPage::scanSongs()
{
    QDir dir(QCoreApplication::applicationDirPath() + Config::MUSIC_DIR_SUFFIX);
    if (!dir.exists()) {
        ui->listWidget_lyric->addItem("未找到音乐目录");
        return;
    }

    QStringList filters;
    filters << "*.mp3";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    m_songList.clear();
    m_playlist->clear();

    for (const QFileInfo &info : files) {
        SongInfo song;
        song.filePath    = info.absoluteFilePath();
        QString baseName = info.completeBaseName(); // 不含 .mp3 的文件名

        // 尝试按 " - " 分割歌手和歌名
        QStringList parts = baseName.split(" - ");
        if (parts.size() >= 2) {
            song.title    = parts[0].trimmed();
            song.singer   = parts[1].trimmed();
            song.fileName = baseName; // 保留原始名（可选）
        } else {
            // 没有分隔符，则歌手未知，歌名就是整个文件名
            song.singer   = "未知歌手";
            song.title    = baseName;
            song.fileName = baseName;
        }

        // 尝试查找同名歌词
        QString lrcPath = info.absolutePath() + "/" + baseName + ".lrc";
        if (QFile::exists(lrcPath))
            song.lyricPath = lrcPath;

        // 尝试查找同名封面
        QString coverPathPng = info.absolutePath() + "/" + baseName + ".png";
        QString coverPathJpg = info.absolutePath() + "/" + baseName + ".jpg";
        if (QFile::exists(coverPathPng))
            song.coverPath = coverPathPng;
        else if (QFile::exists(coverPathJpg))
            song.coverPath = coverPathJpg;

        m_songList.append(song);
        m_playlist->addMedia(QUrl::fromLocalFile(song.filePath));
    }

    if (m_songList.isEmpty()) {
        ui->listWidget_lyric->addItem("没有找到 MP3 文件，请将歌曲放入music目录");
    } else {
        QString lastSongPath = ConfigManager::instance()->currentSongPath();
        int restoreIndex     = -1;
        if (!lastSongPath.isEmpty()) {
            for (int i = 0; i < m_songList.size(); ++i) {
                if (m_songList[i].filePath == lastSongPath) {
                    restoreIndex = i;
                    break;
                }
            }
        }
        if (restoreIndex >= 0) {
            m_playlist->setCurrentIndex(restoreIndex);
        } else {
            m_playlist->setCurrentIndex(0); // 默认第一首
        }
    }
}

void MusicPage::loadLyrics(const QString &lyricPath)
{
    m_lyricLines.clear();
    if (ui->listWidget_lyric)
        ui->listWidget_lyric->clear();

    QFile file(lyricPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ui->listWidget_lyric) ui->listWidget_lyric->addItem("无法打开歌词文件");
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    QRegularExpression timeRegex("\\[(\\d{2}):(\\d{2})(?:\\.(\\d{2,3}))?\\]");
    QRegularExpression metaRegex("\\[(\\w+):([^\\]]+)\\]");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 跳过纯元数据行
        if (metaRegex.match(line).hasMatch() && !line.contains(timeRegex))
            continue;

        QList<qint64> times;
        QRegularExpressionMatchIterator it = timeRegex.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int minute                    = match.captured(1).toInt();
            int second                    = match.captured(2).toInt();
            int msec                      = 0;
            QString msecStr               = match.captured(3);
            if (!msecStr.isEmpty()) {
                while (msecStr.length() < 3) msecStr += "0";
                msec = msecStr.toInt();
            }
            times.append(minute * 60000 + second * 1000 + msec);
        }
        if (times.isEmpty()) continue;

        QString text = line;
        text.remove(timeRegex);
        text = text.trimmed();
        if (text.isEmpty()) continue;

        for (qint64 t : times) {
            m_lyricLines.append({t, text});
        }
    }
    file.close();

    if (m_lyricLines.isEmpty()) {
        if (ui->listWidget_lyric) ui->listWidget_lyric->addItem("无有效歌词");
        return;
    }

    // 按时间排序
    std::sort(m_lyricLines.begin(), m_lyricLines.end(),
              [](const LyricItem &a, const LyricItem &b) { return a.time < b.time; });

    // 填充 QListWidget（只显示文本）
    for (const LyricItem &item : m_lyricLines) {
        QListWidgetItem *listItem = new QListWidgetItem(item.text);
        listItem->setTextAlignment(Qt::AlignCenter);
        ui->listWidget_lyric->addItem(listItem);
    }

    m_currentLyricIndex = -1;
}

void MusicPage::updateCover(const QString &coverPath)
{
    if (!coverPath.isEmpty() && QFile::exists(coverPath)) {
        QPixmap pix(coverPath);
        if (!pix.isNull()) {
            QIcon icon(pix.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            ui->icon_cover->setIcon(icon);
            ui->icon_cover->setIconSize(QSize(256, 256));
            return;
        }
    }
    // 默认封面
    ui->icon_cover->setIcon(QIcon(":/images/images/kunkun.png"));
}

void MusicPage::updateSongInfo(int index)
{
    if (index < 0 || index >= m_songList.size())
        return;
    const SongInfo &song = m_songList[index];
    // 显示歌名（或 歌手 - 歌名，根据需要）
    ui->label_songName->setText(song.title);
    ui->label_singer->setText(song.singer);
    updateCover(song.coverPath);
    if (!song.lyricPath.isEmpty())
        loadLyrics(song.lyricPath);
    else {
        ui->listWidget_lyric->clear();
        ui->listWidget_lyric->addItem("暂无歌词");
    }
}

void MusicPage::onPlayPause()
{
    if (m_player->state() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void MusicPage::onPrevious()
{
    m_playlist->previous();
}

void MusicPage::onNext()
{
    m_playlist->next();
}

void MusicPage::onProgressSliderMoved(int value)
{
    m_player->setPosition(value * 1000);
}

void MusicPage::onPlayModeClicked()
{
    int nextMode;
    switch (m_playMode) {
        case QMediaPlaylist::Sequential:
            nextMode = 1;
            break; // → 列表循环
        case QMediaPlaylist::Loop:
            nextMode = 2;
            break; // → 单曲循环
        case QMediaPlaylist::CurrentItemInLoop:
            nextMode = 3;
            break; // → 随机播放
        case QMediaPlaylist::Random:
            nextMode = 0;
            break; // → 顺序播放
        default:
            nextMode = 1;
            break;
    }
    setPlayMode(nextMode);
    ConfigManager::instance()->setPlayMode(nextMode);
}

void MusicPage::onPlayListClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle("播放列表");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QListWidget *list   = new QListWidget(&dialog);
    for (const SongInfo &song : m_songList)
        list->addItem(song.fileName);
    list->setCurrentRow(m_playlist->currentIndex());
    layout->addWidget(list);
    QPushButton *okBtn = new QPushButton("切换", &dialog);
    layout->addWidget(okBtn);
    connect(okBtn, &QPushButton::clicked, &dialog, [this, list, &dialog]() {
        int row = list->currentRow();
        if (row >= 0 && row < m_songList.size()) {
            m_playlist->setCurrentIndex(row);
            m_player->play();
        }
        dialog.accept();
    });
    dialog.exec();
}

void MusicPage::onStateChanged(QMediaPlayer::State state)
{
    if (state == QMediaPlayer::PlayingState) {
        ui->toolButton_play->setIcon(QIcon(":/images/images/暂停.png"));
        IconHelper::setupButton(ui->toolButton_play, Qt::black);
    } else {
        ui->toolButton_play->setIcon(QIcon(":/images/images/播放.png"));
        IconHelper::setupButton(ui->toolButton_play, Qt::black);
    }
    emit stateChanged(state == QMediaPlayer::PlayingState);
}

void MusicPage::onDurationChanged(qint64 duration)
{
    qint64 seconds = duration / 1000;
    ui->slider_progress->setRange(0, seconds);
    QTime time(0, 0, 0);
    time = time.addSecs(seconds);
    ui->label_totalTime->setText(time.toString("mm:ss"));
}

void MusicPage::onPositionChanged(qint64 position)
{
    if (!ui->slider_progress->isSliderDown())
        ui->slider_progress->setValue(position / 1000);
    QTime time(0, 0, 0);
    time = time.addSecs(position / 1000);
    ui->label_currentTime->setText(time.toString("mm:ss"));
    updateLyric(position);
}

void MusicPage::onCurrentMediaChanged(int index)
{
    if (index < 0 || index >= m_songList.size())
        return;
    updateSongInfo(index);
    ConfigManager::instance()->setCurrentSongPath(m_songList[index].filePath);
    emit mediaChanged(m_songList[index]);
}

void MusicPage::updateLyric(qint64 position)
{
    if (m_lyricLines.isEmpty() || !ui->listWidget_lyric)
        return;

    // 找到当前时间对应的歌词索引
    int newIndex = -1;
    for (int i = 0; i < m_lyricLines.size(); ++i) {
        if (m_lyricLines[i].time <= position)
            newIndex = i;
        else
            break;
    }

    if (newIndex == m_currentLyricIndex)
        return;

    m_currentLyricIndex = newIndex;

    // 1. 清除所有歌词的自定义样式（恢复默认）
    for (int i = 0; i < ui->listWidget_lyric->count(); ++i) {
        QListWidgetItem *item = ui->listWidget_lyric->item(i);
        if (item) {
            // 恢复默认前景色和背景色（根据你的主题调整）
            item->setForeground(QBrush(Qt::black));
            // 可选：恢复默认字体（不加粗）
            QFont font = item->font();
            font.setBold(false);
            item->setFont(font);
        }
    }

    if (m_currentLyricIndex < 0)
        return;

    // 2. 找出与当前时间相同的所有连续行（支持多行歌词）
    qint64 currentTime = m_lyricLines[m_currentLyricIndex].time;
    int startIdx       = m_currentLyricIndex;
    while (startIdx > 0 && m_lyricLines[startIdx - 1].time == currentTime)
        startIdx--;
    int endIdx = m_currentLyricIndex;
    while (endIdx + 1 < m_lyricLines.size() && m_lyricLines[endIdx + 1].time == currentTime)
        endIdx++;

    // 3. 为当前歌词块设置高亮样式
    for (int i = startIdx; i <= endIdx; ++i) {
        QListWidgetItem *item = ui->listWidget_lyric->item(i);
        if (item) {
            // 设置高亮颜色
            item->setForeground(QBrush(Qt::red));
            // 可选：加粗字体
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
    }

    // 4. 滚动到当前歌词块的可视区域（以 startIdx 为基准）
    if (startIdx >= 0 && startIdx < ui->listWidget_lyric->count()) {
        ui->listWidget_lyric->scrollToItem(ui->listWidget_lyric->item(startIdx), QAbstractItemView::PositionAtCenter);
    }
}

void MusicPage::onLyricItemClicked(QListWidgetItem *item)
{
    // 如果没有歌词数据或播放器无效，直接返回
    if (m_lyricLines.isEmpty() || !m_player)
        return;

    // 获取点击的行索引
    int row = ui->listWidget_lyric->row(item);
    if (row < 0 || row >= m_lyricLines.size())
        return;

    // 获取该行对应的时间（毫秒）
    qint64 targetTime = m_lyricLines[row].time;

    // 设置播放位置
    m_player->setPosition(targetTime);

    // 可选：如果当前是暂停状态，自动开始播放（根据你的需求决定）
    // if (m_player->state() == QMediaPlayer::PausedState)

    // 注意：设置位置后会触发 positionChanged 信号，
    // 进而自动调用 updateLyric() 刷新高亮，无需额外代码。
}