#include "mainwindow.h"
#include "rearViewCamera.h"
#include "configmanager.h"
#include "homePage.h"
#include "iconhelper.h"
#include "musicPage.h"
#include "navigationPage.h"
#include "settingsPage.h"
#include "toolsPage.h"
#include "ui_mainwindow.h"
#include "vehicledataprocessor.h"
#include "volumemanager.h"
#include "ec20manager.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 加载样式表
    QFile styleFile(":/styles/styles/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        setStyleSheet(style);
    }

    // 初始化页面
    setupPages();

    // 设置图标
    setupIcons();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupPages()
{
    // 加载配置
    int savedVolume = ConfigManager::instance()->volume();
    ui->sliderVolume->setValue(savedVolume);

    // 同步时间显示
    QTimer *timeTimer = new QTimer(this);
    connect(timeTimer, &QTimer::timeout, this, [=]() {
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timeString        = currentDateTime.toString("MM-dd hh:mm");
        ui->label_time->setText(timeString);
    });
    timeTimer->start(1000);

    // 网络时间同步（WiFi/4G 连接时自动触发）
    auto syncNetworkTime = []() {
        QProcess::startDetached("sh", {"-c", "(ntpdate -s pool.ntp.org 2>/dev/null || rdate -s time.nist.gov 2>/dev/null) &"});
    };

    // 同步 WiFi 状态
    WifiManager *wifi = WifiManager::instance();
    auto setWifiIcon  = [this, syncNetworkTime](bool connected) {
        ui->icon_WiFi->setIcon(QIcon(connected ? ":/images/images/WiFi.png"
                                                : ":/images/images/WiFi-关闭.png"));
        IconHelper::setColor(ui->icon_WiFi, Qt::black);
        if (connected) syncNetworkTime();
    };
    setWifiIcon(wifi->isConnected());
    connect(wifi, &WifiManager::connectionSuccess, this, [setWifiIcon](const QString &) { setWifiIcon(true); });
    connect(wifi, &WifiManager::disconnected, this, [setWifiIcon] { setWifiIcon(false); });

    // 同步 4G 状态
    EC20Manager *ec20 = EC20Manager::instance();
    auto set4GIcon    = [this, syncNetworkTime](bool connected) {
        ui->icon_4G->setIcon(QIcon(connected ? ":/images/images/信号-4G.png"
                                                : ":/images/images/信号-无.png"));
        IconHelper::setColor(ui->icon_4G, Qt::black);
        if (connected) syncNetworkTime();
    };
    set4GIcon(ec20->isDataConnected());
    connect(ec20, &EC20Manager::dataConnectionStatusChanged, this,
            [set4GIcon](bool connected, const QString &) { set4GIcon(connected); });

    // 音量调整滑块配置
    QTimer *hideTimer = new QTimer(this);
    hideTimer->setSingleShot(true);
    hideTimer->setInterval(5000);
    connect(hideTimer, &QTimer::timeout, this,
            [=]() { ui->stackedWidget_sliderVolume->setCurrentIndex(0); });
    connect(ui->toolButton_volume, &QToolButton::clicked, this, [=]() {
        ui->stackedWidget_sliderVolume->setCurrentIndex(1);
        hideTimer->start();
    });
    connect(ui->sliderVolume, &QSlider::valueChanged, this, [=](int value) {
        hideTimer->start();
        updateVolumeIcon(value);
        VolumeManager::instance()->setVolume(value);
    });
    connect(ui->sliderVolume, &QSlider::sliderReleased, this, [=]() {
        int finalValue = ui->sliderVolume->value();
        ConfigManager::instance()->setVolume(finalValue);
    });
    connect(VolumeManager::instance(), &VolumeManager::volumeChanged, this, [=](int value) {
        if (ui->sliderVolume->value() != value) {
            ui->sliderVolume->setValue(value);
        }
    });

    // 加载全部页面（启动时一次性创建）
    m_homePage           = new HomePage(this);
    m_musicPage          = new MusicPage(this);
    m_toolsPage          = new ToolsPage(this);
    m_navigationPage     = new NavigationPage(this);
    m_settingsPage       = new SettingsPage(this);
    m_rearViewCameraPage = new RearViewCamera(nullptr); // 独立顶层窗口，覆盖全屏

    m_homePage->setMusicController(m_musicPage);
    connect(EC20Manager::instance(), &EC20Manager::positionUpdated,
            m_navigationPage, &NavigationPage::onPositionUpdated);

    ui->stackedWidget->addWidget(m_homePage);
    ui->stackedWidget->addWidget(m_musicPage);
    ui->stackedWidget->addWidget(m_toolsPage);
    ui->stackedWidget->addWidget(m_navigationPage);
    ui->stackedWidget->addWidget(m_settingsPage);
    ui->stackedWidget->setCurrentWidget(m_homePage);

    connect(ui->toolButton_home, &QToolButton::clicked, this,
            [this]() { ui->stackedWidget->setCurrentWidget(m_homePage); });
    connect(ui->toolButton_tools, &QToolButton::clicked, this,
            [this]() { ui->stackedWidget->setCurrentWidget(m_toolsPage); });
    connect(ui->toolButton_navigation, &QToolButton::clicked, this,
            [this]() { ui->stackedWidget->setCurrentWidget(m_navigationPage); });
    connect(ui->toolButton_music, &QToolButton::clicked, this,
            [this]() { ui->stackedWidget->setCurrentWidget(m_musicPage); });
    connect(ui->toolButton_settings, &QToolButton::clicked, this,
            [this]() { ui->stackedWidget->setCurrentWidget(m_settingsPage); });

    // 连接VehicleDataProcessor到页面
    VehicleDataProcessor *vdp = VehicleDataProcessor::instance();
    connect(vdp, &VehicleDataProcessor::speedChanged, m_homePage,
            &HomePage::updateSpeed);
    connect(vdp, &VehicleDataProcessor::rpmChanged, m_homePage,
            &HomePage::updateRpm);
    connect(vdp, &VehicleDataProcessor::oilChanged, m_homePage,
            &HomePage::updateOil);
    connect(vdp, &VehicleDataProcessor::temperatureChanged, m_homePage,
            &HomePage::updateTemperature);
    connect(vdp, &VehicleDataProcessor::humidityChanged, m_homePage,
            &HomePage::updateHumidity);
    connect(vdp, &VehicleDataProcessor::gearChanged, m_rearViewCameraPage,
            &RearViewCamera::onGearChanged);
    connect(vdp, &VehicleDataProcessor::distanceChanged, m_rearViewCameraPage,
            &RearViewCamera::onDistanceChanged);
}

void MainWindow::setupIcons()
{
    updateVolumeIcon(VolumeManager::instance()->volume());

    IconHelper::setupButton(ui->toolButton_home, Qt::black);
    IconHelper::setupButton(ui->toolButton_tools, Qt::black);
    IconHelper::setupButton(ui->toolButton_navigation, Qt::black);
    IconHelper::setupButton(ui->toolButton_music, Qt::black);
    IconHelper::setupButton(ui->toolButton_settings, Qt::black);
    IconHelper::setColor(ui->icon_battery, Qt::black);
    IconHelper::setColor(ui->icon_4G, Qt::black);
    IconHelper::setColor(ui->icon_WiFi, Qt::black);
}

void MainWindow::updateVolumeIcon(int volume)
{
    static const QIcon kHigh(":/images/images/音量_高.png");
    static const QIcon kMid(":/images/images/音量_中.png");
    static const QIcon kLow(":/images/images/音量_低.png");
    static const QIcon kMute(":/images/images/音量_静音.png");

    const QIcon &icon = volume > 66 ? kHigh : volume > 33 ? kMid
                                          : volume > 0    ? kLow
                                                          : kMute;
    ui->toolButton_volume->setIcon(icon);
    IconHelper::setupButton(ui->toolButton_volume, Qt::black);
}