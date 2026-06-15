#include "homePage.h"
#include "ui_homePage.h"
#include "iconhelper.h"
#include "weathermanager.h"
#include "musicPage.h"
#include <QFile>
#include <QDebug>

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HomePage)
{
    ui->setupUi(this);

    setupIcons();

    WeatherManager *nm = WeatherManager::instance();
    connect(nm, &WeatherManager::weatherDataReady, this, &HomePage::onWeatherUpdate);
    connect(nm, &WeatherManager::forecastReady, this, &HomePage::onForecastUpdate);
    connect(nm, &WeatherManager::errorOccurred, this, &HomePage::onNetworkError);

    // 自动定位并请求天气
    nm->requestWeather();

    // 定时每30分钟刷新一次
    QTimer *refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, [nm]() {
        nm->requestWeather();
    });
    refreshTimer->start(30 * 60 * 1000);
}

HomePage::~HomePage()
{
    delete ui;
}

void HomePage::setupIcons()
{
    IconHelper::setupButton(ui->toolButton_previous, Qt::black);
    IconHelper::setupButton(ui->toolButton_play, Qt::black);
    IconHelper::setupButton(ui->toolButton_next, Qt::black);
}

// ----------- 公共接口 -----------
void HomePage::setMusicController(MusicPage *musicPage)
{
    if (!musicPage) return;
    connect(ui->toolButton_previous, &QToolButton::clicked,
            musicPage, &MusicPage::onPrevious);
    connect(ui->toolButton_play, &QToolButton::clicked,
            musicPage, &MusicPage::onPlayPause);
    connect(ui->toolButton_next, &QToolButton::clicked,
            musicPage, &MusicPage::onNext);
    connect(musicPage, &MusicPage::stateChanged,
            this, [this](bool isPlaying) {
                if (isPlaying) {
                    ui->toolButton_play->setIcon(QIcon(":/images/images/暂停.png"));
                } else {
                    ui->toolButton_play->setIcon(QIcon(":/images/images/播放.png"));
                }
                IconHelper::setupButton(ui->toolButton_play, Qt::black);
            });
    connect(musicPage, &MusicPage::mediaChanged, this, [this](const SongInfo &info) {
        ui->label_songName->setText(info.title);
        if (!info.coverPath.isEmpty() && QFile::exists(info.coverPath))
            ui->icon_homeMusic->setIcon(QIcon(info.coverPath));
        else
            ui->icon_homeMusic->setIcon(QIcon(":/images/images/kunkun.png"));
    });
    // 手动触发一次歌曲信息更新
    SongInfo currentSong = musicPage->getCurrentSongInfo();
    ui->label_songName->setText(currentSong.title);
    if (!currentSong.coverPath.isEmpty() && QFile::exists(currentSong.coverPath))
        ui->icon_homeMusic->setIcon(QIcon(currentSong.coverPath));
    else
        ui->icon_homeMusic->setIcon(QIcon(":/images/images/kunkun.png"));
    // 手动触发一次播放状态更新
    bool playing = musicPage->isPlaying();
    if (playing)
        ui->toolButton_play->setIcon(QIcon(":/images/images/暂停.png"));
    else
        ui->toolButton_play->setIcon(QIcon(":/images/images/播放.png"));
    IconHelper::setupButton(ui->toolButton_play, Qt::black);
}

void HomePage::updateSpeed(int speed)
{
    m_speed = speed;
    ui->label_varSpeed->setText(QStringLiteral("<html><head/><body><p align=\"center\"><span style=\" font-weight:600;\">%1</span></p></body></html>").arg(speed));
}

void HomePage::updateRpm(int rpm)
{
    m_rpm = rpm;
    ui->label_varRpm->setText(QStringLiteral("<html><head/><body><p align=\"center\"><span style=\" font-weight:600;\">%1</span></p></body></html>")
                                  .arg(rpm));
}

void HomePage::updateOil(int oilX10)
{
    m_oil = oilX10;
    ui->label_varOil->setText(QStringLiteral("<html><head/><body><p align=\"center\"><span style=\" font-weight:600;\">%1%</span></p></body></html>")
                                  .arg(oilX10 / 10.0, 0, 'f', 1));
}

void HomePage::updateTemperature(int tempX10)
{
    m_temperature = tempX10;
    ui->label_varTemp->setText(QStringLiteral("<html><head/><body><p align=\"center\"><span style=\" font-weight:600;\">%1°C</span></p></body></html>")
                                   .arg(tempX10 / 10.0, 0, 'f', 1));
}

void HomePage::updateHumidity(int humX10)
{
    m_humidity = humX10;
    ui->label_varHum->setText(QStringLiteral("<html><head/><body><p align=\"center\"><span style=\" font-weight:600;\">%1%</span></p></body></html>")
                                  .arg(humX10 / 10.0, 0, 'f', 1));
}

void HomePage::showMessage(const QString &msg)
{
    qDebug() << "[首页]" << msg;
    ui->label_songName->setToolTip(msg);
}

static QIcon getWeatherIcon(const QString &weatherText)
{
    QString iconPath;
    if (weatherText.contains("晴"))
        iconPath = ":/images/images/天气-晴天.png";
    else if (weatherText.contains("多云"))
        iconPath = ":/images/images/天气-多云.png";
    else if (weatherText.contains("阴"))
        iconPath = ":/images/images/天气-阴天.png";
    else if (weatherText.contains("雨"))
        iconPath = ":/images/images/天气-雨天.png";
    else if (weatherText.contains("雪"))
        iconPath = ":/images/images/天气-雪天.png";
    else if (weatherText.contains("雷"))
        iconPath = ":/images/images/天气-雷电天气.png";
    else
        iconPath = ":/images/images/天气-阴天.png";
    return QIcon(iconPath);
}

void HomePage::onWeatherUpdate(const QString &city, int temp, const QString &text)
{
    ui->label_weatherTemp->setText(QStringLiteral("<html><head/><body><p align=\"center\"><span style=\" font-weight:600;\">%1°C</span></p></body></html>").arg(temp));
    ui->label_city->setText(QStringLiteral("<html><head/><body><p align=\"center\">%1</p></body></html>").arg(city));
    ui->icon_weather->setIcon(getWeatherIcon(text));
    qDebug() << "天气更新:" << city << temp << text;
}

void HomePage::onForecastUpdate(const QStringList &dates,
                                const QList<int> &highs,
                                const QList<int> &lows,
                                const QStringList &texts)
{
    if (dates.size() >= 1) {
        QString today = QString("%1~%2°C").arg(lows[0]).arg(highs[0]);
        ui->label_todayTemp->setText(today);
        ui->label_today->setText(dates.value(0) + " 今天");
        ui->icon_weather_today->setIcon(getWeatherIcon(texts.value(0)));
        ui->label_todayTemp_1->setText(QStringLiteral("<html><head/><body><p align=\"center\">%1 %2</p></body></html>")
                                           .arg(texts.value(0), today));
    }
    if (dates.size() >= 2) {
        QString tomorrow = QString("%1~%2°C").arg(lows[1]).arg(highs[1]);
        ui->label_tomorrowTemp->setText(tomorrow);
        ui->label_tomorrow->setText(dates.value(1) + " 明天");
        ui->icon_weather_tomorrow->setIcon(getWeatherIcon(texts.value(1)));
    }
    if (dates.size() >= 3) {
        QString after = QString("%1~%2°C").arg(lows[2]).arg(highs[2]);
        ui->label_afterTomorrowTemp->setText(after);
        ui->label_afterTomorrow->setText(dates.value(2) + " 后天");
        ui->icon_weather_afterTomorrow->setIcon(getWeatherIcon(texts.value(2)));
    }
    qDebug() << "预报更新:" << dates << highs << lows << texts;
}

void HomePage::onNetworkError(const QString &error)
{
    ui->label_weatherTemp->setText("--°C");
    ui->label_todayTemp_1->setText("网络错误");
    ui->icon_weather->setIcon(QIcon(":/images/images/天气-阴天.png"));
    ui->icon_weather_today->setIcon(QIcon(":/images/images/天气-阴天.png"));
    ui->icon_weather_tomorrow->setIcon(QIcon(":/images/images/天气-阴天.png"));
    ui->icon_weather_afterTomorrow->setIcon(QIcon(":/images/images/天气-阴天.png"));
    ui->label_todayTemp->setText("--~--°C");
    ui->label_tomorrowTemp->setText("--~--°C");
    ui->label_afterTomorrowTemp->setText("--~--°C");
    ui->label_today->setText("今天");
    ui->label_tomorrow->setText("明天");
    ui->label_afterTomorrow->setText("后天");
    qDebug() << "HomePage 网络错误:" << error;
}