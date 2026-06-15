#include "settingsPage.h"
#include "ui_settingsPage.h"
#include "volumemanager.h"
#include "configmanager.h"
#include "ec20manager.h"
#include "MySlider.h"
#include <QScroller>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QInputDialog>
#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>
#include <QComboBox>
#include <QTimer>
#include <QApplication>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
    , m_wifiManager(WifiManager::instance())
{
    ui->setupUi(this);

    enableTouchScrolling(); // 启用触摸滚动

    setupLeftArea();
    setupPage0();
    setupPage1();
    setupPage2();
    setupPage3();
    setupPage4();
    setupPage5();
    setupPage6();

    // WiFi 可用且已开启时自动扫描
    if (m_wifiManager->isWifiAvailable() && m_wifiManager->isWifiEnabled())
        m_wifiManager->scanNetworks();
}

SettingsPage::~SettingsPage()
{
    delete ui;
}

// --------------------- 页面设置 ---------------------
// 左侧导航栏
void SettingsPage::setupLeftArea()
{
    QVBoxLayout *btnLayout = new QVBoxLayout(ui->scrollAreaWidgetContents_left);
    QButtonGroup *btnGroup = new QButtonGroup(this);
    btnGroup->setExclusive(true);
    QStringList categories = {"通用", "外观", "显示", "音频", "WLAN", "系统", "移动网络"};
    for (int i = 0; i < categories.size(); ++i) {
        QPushButton *btn = new QPushButton(categories[i]);
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(
            "QPushButton {"
            "    text-align: left;"
            "    padding: 12px 16px;"
            "    border: 0;"
            "    background-color: transparent;"
            "    font-size: 16px;"
            "    color: #374151;"
            "}"
            "QPushButton:checked {"
            "    background-color: #e5e7eb;"
            "    color: #111827;"
            "    font-weight: 500;"
            "}");
        btnLayout->addWidget(btn);
        btnGroup->addButton(btn, i);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            ui->stackedWidget->setCurrentIndex(i);
        });
    }
    btnLayout->addStretch();
    btnGroup->buttons().first()->setChecked(true);
}

// 通用
void SettingsPage::setupPage0()
{
    QWidget *content    = ui->scrollAreaWidgetContents;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    // --- 按钮 ---
    QFrame *groupBtn       = createGroup("按钮");
    QPushButton *btnNormal = new QPushButton("普通按钮");
    addSettingRow(groupBtn, "QPushButton", "", btnNormal);
    QPushButton *btnCheck = new QPushButton("复选按钮");
    btnCheck->setCheckable(true);
    addSettingRow(groupBtn, "复选按钮", "", btnCheck);
    QPushButton *btnDisabled = new QPushButton("禁用按钮");
    btnDisabled->setEnabled(false);
    addSettingRow(groupBtn, "禁用按钮", "", btnDisabled, true);
    layout->addWidget(groupBtn);

    // --- 开关 & 复选框 ---
    QFrame *groupToggle = createGroup("开关 & 勾选");
    SwitchButton *sw    = new SwitchButton();
    addSettingRow(groupToggle, "SwitchButton", "", sw);
    QCheckBox *cb = new QCheckBox("复选框");
    cb->setChecked(true);
    addSettingRow(groupToggle, "QCheckBox", "", cb, true);
    layout->addWidget(groupToggle);

    // --- 单选 ---
    QFrame *groupRadio = createGroup("单选");
    QRadioButton *rb1  = new QRadioButton("选项 A");
    QRadioButton *rb2  = new QRadioButton("选项 B");
    QRadioButton *rb3  = new QRadioButton("选项 C");
    rb1->setChecked(true);
    QWidget *radioWidget     = new QWidget();
    QHBoxLayout *radioLayout = new QHBoxLayout(radioWidget);
    radioLayout->addWidget(rb1);
    radioLayout->addWidget(rb2);
    radioLayout->addWidget(rb3);
    addSettingRow(groupRadio, "QRadioButton", "", radioWidget, true);
    layout->addWidget(groupRadio);

    // --- 滑块 ---
    QFrame *groupSlider = createGroup("滑块");
    MySlider *sliderH   = new MySlider();
    sliderH->setOrientation(Qt::Horizontal);
    sliderH->setValue(50);
    addSettingRow(groupSlider, "水平滑块", "", sliderH);
    QProgressBar *progress = new QProgressBar();
    progress->setRange(0, 100);
    progress->setValue(70);
    progress->setFixedHeight(20);
    addSettingRow(groupSlider, "QProgressBar", "", progress, true);
    layout->addWidget(groupSlider);

    // --- 下拉 & 数字 ---
    QFrame *groupCombo = createGroup("下拉 & 数字");
    QComboBox *combo   = new QComboBox();
    combo->addItems({"选项一", "选项二", "选项三"});
    addSettingRow(groupCombo, "QComboBox", "", combo);
    QSpinBox *spin = new QSpinBox();
    spin->setRange(0, 100);
    spin->setValue(42);
    spin->setFixedWidth(80);
    addSettingRow(groupCombo, "QSpinBox", "", spin, true);
    layout->addWidget(groupCombo);

    // --- 输入 ---
    QFrame *groupInput  = createGroup("文本输入");
    QLineEdit *lineEdit = new QLineEdit();
    lineEdit->setPlaceholderText("请输入文本...");
    lineEdit->setClearButtonEnabled(true);
    addSettingRow(groupInput, "QLineEdit", "", lineEdit);
    QTextEdit *textEdit = new QTextEdit();
    textEdit->setPlaceholderText("多行文本...");
    textEdit->setFixedHeight(60);
    addSettingRow(groupInput, "QTextEdit", "", textEdit, true);
    layout->addWidget(groupInput);

    layout->addStretch();
}

// 外观
void SettingsPage::setupPage1()
{
    QWidget *content    = ui->scrollAreaWidgetContents_2;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    QFrame *group       = createGroup("主题切换");
    QComboBox *themeBox = new QComboBox;
    QStringList themes  = {"style", "MacOS", "AMOLED", "Aqua", "ConsoleStyle",
                           "ElegantDark", "ManjaroMix", "MaterialDark", "NeonButtons", "Ubuntu"};
    themeBox->addItems(themes);
    themeBox->setCurrentText("style");
    connect(themeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [themes](int idx) {
                QString name = (idx >= 0 && idx < themes.size()) ? themes[idx] : "style";
                QFile qss(QString(":/styles/styles/%1.qss").arg(name));
                if (qss.open(QFile::ReadOnly)) {
                    qApp->setStyleSheet(qss.readAll());
                    qss.close();
                }
            });
    addSettingRow(group, "当前主题", "", themeBox, true);

    layout->addWidget(group);
    layout->addStretch();
}

// 显示
void SettingsPage::setupPage2()
{
    m_backlight = new BacklightControl("/sys/class/backlight/backlight", this);
    if (!m_backlight->isValid()) {
        qDebug() << "Backlight control not available";
    }

    QWidget *content    = ui->scrollAreaWidgetContents_3;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    QFrame *group1             = createGroup("亮度");
    MySlider *brightnessSlider = new MySlider();
    brightnessSlider->setRange(0, 100);
    brightnessSlider->setOrientation(Qt::Horizontal);
    if (m_backlight->isValid()) {
        int max = m_backlight->maxBrightness();
        int cur = m_backlight->currentBrightness();
        if (max > 0) {
            int percent = (cur * 100) / max;
            brightnessSlider->setValue(percent);
        }
        connect(brightnessSlider, &QSlider::valueChanged, this, [this](int percent) {
            m_backlight->setBrightnessPercent(percent);
        });
    } else {
        brightnessSlider->setEnabled(false);
        qDebug() << "亮度调节不可用";
    }
    addSettingRow(group1, "亮度", "", brightnessSlider, true);

    QFrame *group2                  = createGroup("显示");
    QHBoxLayout *radioLayout        = new QHBoxLayout;
    QRadioButton *radioButton_light = new QRadioButton("亮色模式");
    QRadioButton *radioButton_dark  = new QRadioButton("暗色模式");
    radioButton_light->setChecked(true);
    QWidget *radioWidget = new QWidget;
    radioWidget->setLayout(radioLayout);
    radioLayout->addWidget(radioButton_light);
    radioLayout->addWidget(radioButton_dark);
    addSettingRow(group2, "显示模式", "", radioWidget, true);

    layout->addWidget(group1);
    layout->addWidget(group2);
    layout->addStretch();
}

// 音频
void SettingsPage::setupPage3()
{
    QWidget *content    = ui->scrollAreaWidgetContents_4;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    QFrame *group1         = createGroup("音频");
    MySlider *volumeSlider = new MySlider();
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(VolumeManager::instance()->volume());
    volumeSlider->setOrientation(Qt::Horizontal);
    connect(volumeSlider, &QSlider::valueChanged, this, [=](int value) {
        VolumeManager::instance()->setVolume(value);
    });
    connect(volumeSlider, &QSlider::sliderReleased, this, [=]() {
        int finalValue = volumeSlider->value();
        ConfigManager::instance()->setVolume(finalValue);
    });
    connect(VolumeManager::instance(), &VolumeManager::volumeChanged, this, [=](int value) {
        if (volumeSlider->value() != value) {
            volumeSlider->setValue(value);
        }
    });
    addSettingRow(group1, "音量", "", volumeSlider, true);
    layout->addWidget(group1);
    layout->addStretch();
}

// WLAN
void SettingsPage::setupPage4()
{
    // 初始化 Wi-Fi 管理器
    connect(m_wifiManager, &WifiManager::enabledWifi, this, &SettingsPage::onWifiEnabled);
    connect(m_wifiManager, &WifiManager::disabledWifi, this, &SettingsPage::onWifiDisabled);
    connect(m_wifiManager, &WifiManager::scanFinished, this, &SettingsPage::onWifiScanFinished);
    connect(m_wifiManager, &WifiManager::connectionSuccess, this, &SettingsPage::onWifiConnectionSuccess);
    connect(m_wifiManager, &WifiManager::connectionFailed, this, &SettingsPage::onWifiConnectionFailed);
    connect(m_wifiManager, &WifiManager::disconnected, this, &SettingsPage::onWifiDisconnected);
    connect(m_wifiManager, &WifiManager::networkForgotten, this, &SettingsPage::onWifiForgotten);
    connect(m_wifiManager, &WifiManager::errorOccurred, this, &SettingsPage::onWifiError);

    QWidget *content    = ui->scrollAreaWidgetContents_5;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    // 分组1：WLAN 开关
    QFrame *controlGroup = createGroup("WLAN");

    // 检测 WiFi 硬件是否可用
    bool wifiExists    = m_wifiManager->isWifiAvailable();
    bool wifiOn        = wifiExists && m_wifiManager->isWifiEnabled();

    m_wifiEnableSwitch = new SwitchButton();
    m_wifiEnableSwitch->setChecked(wifiOn);
    if (!wifiExists) m_wifiEnableSwitch->setDisabled(true);
    connect(m_wifiEnableSwitch, &SwitchButton::toggled, this, [this](bool on) {
        if (!m_wifiManager->isWifiAvailable()) return;
        onWifiEnableToggle(on);
    });
    addSettingRow(controlGroup, "开启 WLAN", "", m_wifiEnableSwitch);

    m_wifiRefreshBtn = new QPushButton("点击刷新");
    m_wifiRefreshBtn->setFixedWidth(100);
    if (!wifiExists) {
        m_wifiRefreshBtn->setDisabled(true);
    } else if (!wifiOn) {
        m_wifiRefreshBtn->setEnabled(false);
    }
    connect(m_wifiRefreshBtn, &QPushButton::clicked, this, &SettingsPage::onRefreshClicked);
    addSettingRow(controlGroup, "刷新网络", "", m_wifiRefreshBtn);

    if (!wifiExists) {
        m_wifiStatusLabel = new QLabel("WiFi 模块不可用");
    } else if (!wifiOn) {
        m_wifiStatusLabel = new QLabel("WLAN 未开启");
    } else {
        m_wifiStatusLabel = new QLabel("点击刷新扫描网络");
    }
    m_wifiStatusLabel->setStyleSheet("color: #6b7280; padding: 8px 16px;");
    controlGroup->layout()->addWidget(m_wifiStatusLabel);

    // 分组2：可用网络列表
    QFrame *listGroup = createGroup("可用网络");

    // 创建一个容器 widget 用于放置网络列表
    QWidget *listContainer = new QWidget;
    m_wifiListLayout       = new QVBoxLayout(listContainer);
    m_wifiListLayout->setContentsMargins(0, 0, 0, 0);
    m_wifiListLayout->setSpacing(0);

    // 将容器添加到 listGroup 的布局中（注意：createGroup 内部已有 QVBoxLayout）
    listGroup->layout()->addWidget(listContainer);

    // 初始占位提示
    QLabel *emptyLabel = new QLabel("暂无网络，请点击刷新");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #9ca3af; padding: 20px;");
    m_wifiListLayout->addWidget(emptyLabel);

    layout->addWidget(controlGroup);
    layout->addWidget(listGroup);
    layout->addStretch();
}

// 系统
void SettingsPage::setupPage5()
{
    QWidget *content    = ui->scrollAreaWidgetContents_6;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    // 动态获取系统信息
    QString kernelVer = "未知";
#ifdef Q_OS_LINUX
    QFile versionFile("/proc/version");
    if (versionFile.open(QIODevice::ReadOnly)) {
        kernelVer = QString::fromUtf8(versionFile.readLine()).section(' ', 0, 2);
        versionFile.close();
    }
#endif

    long totalMemKB = 0;
#ifdef Q_OS_LINUX
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly)) {
        QString line = QString::fromUtf8(meminfo.readLine());
        meminfo.close();
        QRegularExpression re("MemTotal:\\s*(\\d+)");
        QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch()) totalMemKB = m.captured(1).toLong();
    }
#endif

    QString qtVer  = QT_VERSION_STR;

    QFrame *group1 = createGroup("系统信息");
    addSettingRow(group1, "版本", "1.0.0");
    addSettingRow(group1, "内核", kernelVer);
    addSettingRow(group1, "Qt版本", qtVer);
    addSettingRow(group1, "作者", "Hajimi", nullptr, true);

    QFrame *group2 = createGroup("设备信息");
    addSettingRow(group2, "主机名", QSysInfo::machineHostName());
#ifdef __arm__
    addSettingRow(group2, "架构", "ARM");
#else
    addSettingRow(group2, "架构", QSysInfo::currentCpuArchitecture());
#endif
    if (totalMemKB > 0)
        addSettingRow(group2, "内存", QString("%1 MB").arg(totalMemKB / 1024));
    else
        addSettingRow(group2, "内存", "未知");
    addSettingRow(group2, "平台", QSysInfo::productType(), nullptr, true);

    layout->addWidget(group1);
    layout->addWidget(group2);
    layout->addStretch();
}

// 移动网络
void SettingsPage::setupPage6()
{
    QWidget *content    = ui->scrollAreaWidgetContents_7;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 20);
    layout->setSpacing(0);

    EC20Manager *ec20 = EC20Manager::instance();
    bool moduleOnline = ec20->isOnline();

    // --- 移动数据 ---
    QFrame *groupData     = createGroup("移动数据");
    SwitchButton *swData  = new SwitchButton;
    QLabel *labelDataIp   = new QLabel;
    QLabel *labelDataStat = new QLabel;

    bool dataConnected    = ec20->isDataConnected();
    swData->setChecked(dataConnected);
    labelDataIp->setText(dataConnected ? ec20->getLocalIp() : "—");
    labelDataStat->setText(dataConnected ? "已连接" : "未连接");

    if (!moduleOnline) {
        swData->setDisabled(true);
        labelDataStat->setText("模块离线");
    }

    connect(swData, &SwitchButton::toggled, this, [ec20](bool on) {
        if (!ec20->isOnline()) return;
        ConfigManager::instance()->setValue("mobile_data/enabled", on);
        if (on)
            ec20->startQuectelCM();
        else
            ec20->stopQuectelCM();
    });
    connect(ec20, &EC20Manager::dataConnectionStatusChanged, this,
            [labelDataIp, labelDataStat, swData](bool connected, const QString &ip) {
                swData->blockSignals(true);
                swData->setChecked(connected);
                swData->blockSignals(false);
                labelDataStat->setText(connected ? "已连接" : "未连接");
                labelDataIp->setText(connected ? ip : "—");
            });

    addSettingRow(groupData, "开启移动网络", "", swData);
    addSettingRow(groupData, "状态", "", labelDataStat);
    addSettingRow(groupData, "IP 地址", "", labelDataIp, true);
    layout->addWidget(groupData);

    // --- GPS ---
    QFrame *groupGps    = createGroup("GPS");
    SwitchButton *swGps = new SwitchButton;
    QLabel *labelGpsFix = new QLabel;

    bool gpsEnabled     = ConfigManager::instance()->value("gps/enabled").toBool();
    swGps->setChecked(gpsEnabled);
    labelGpsFix->setText(ec20->isGpsFixed() ? "已定位" : "未定位");

    if (!moduleOnline) {
        swGps->setDisabled(true);
        labelGpsFix->setText("模块离线");
    }

    connect(swGps, &SwitchButton::toggled, this, [ec20](bool on) {
        if (!ec20->isOnline()) return;
        ConfigManager::instance()->setValue("gps/enabled", on);
        if (on)
            ec20->startGps();
        else
            ec20->stopGps();
    });
    connect(ec20, &EC20Manager::gpsStatusChanged, this,
            [labelGpsFix](bool fixed) {
                labelGpsFix->setText(fixed ? "已定位" : "未定位");
            });

    addSettingRow(groupGps, "开启 GPS", "", swGps);
    addSettingRow(groupGps, "定位状态", "", labelGpsFix, true);
    layout->addWidget(groupGps);

    // --- 模块信息 ---
    QFrame *groupInfo = createGroup("模块信息");
    QString manufacturer, model, revision;
    ec20->getModuleInfo(manufacturer, model, revision);
    addSettingRow(groupInfo, "制造商", manufacturer);
    addSettingRow(groupInfo, "型号", model);
    addSettingRow(groupInfo, "固件版本", revision);
    addSettingRow(groupInfo, "IMEI", ec20->getImei());

    QLabel *labelSim = new QLabel;
    auto sim         = ec20->getSimStatus();
    switch (sim) {
        case EC20Manager::SimReady:
            labelSim->setText("已就绪");
            break;
        case EC20Manager::SimNotInserted:
            labelSim->setText("未插入");
            break;
        case EC20Manager::SimPinRequired:
            labelSim->setText("需要 PIN 码");
            break;
        case EC20Manager::SimPukRequired:
            labelSim->setText("需要 PUK 码");
            break;
        default:
            labelSim->setText("未知");
            break;
    }
    addSettingRow(groupInfo, "SIM 卡", "", labelSim);
    addSettingRow(groupInfo, "ICCID", ec20->getIccid());

    QLabel *labelSig = new QLabel;
    int rssi, ber;
    if (ec20->getSignalQuality(rssi, ber))
        labelSig->setText(QString("%1 dBm").arg(rssi));
    else
        labelSig->setText("—");
    addSettingRow(groupInfo, "信号强度", "", labelSig, true);

    layout->addWidget(groupInfo);
    layout->addStretch();
}

// --------------------- 辅助函数 ---------------------
// 启用触摸滚动
void SettingsPage::enableTouchScrolling()
{
    QList<QScrollArea *> scrollAreas = {
        ui->scrollArea_left,
        ui->scrollArea,
        ui->scrollArea_2,
        ui->scrollArea_3,
        ui->scrollArea_4,
        ui->scrollArea_5,
        ui->scrollArea_6, ui->scrollArea_7};

    for (QScrollArea *sa : scrollAreas) {
        QScroller::grabGesture(sa->viewport(), QScroller::LeftMouseButtonGesture);
        QScrollerProperties sp;
        sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0);
        QScroller::scroller(sa->viewport())->setScrollerProperties(sp);
    }
}

// 创建一个分组卡片（带标题）
QFrame *SettingsPage::createGroup(const QString &title)
{
    QFrame *group = new QFrame;
    group->setObjectName("settingGroup");
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(0);
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("groupTitle");
    layout->addWidget(titleLabel);
    return group;
}

// 向分组中添加一行设置项（标签 + 值文本 或 自定义控件）
void SettingsPage::addSettingRow(QFrame *group, const QString &label, const QString &value, QWidget *customWidget, bool lastRow)
{
    QWidget *row = new QWidget;
    row->setObjectName("settingRow");
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(16, 12, 16, 12);
    QLabel *labelWidget = new QLabel(label);
    labelWidget->setObjectName("settingLabel");
    rowLayout->addWidget(labelWidget);
    rowLayout->addStretch();
    if (customWidget) {
        rowLayout->addWidget(customWidget);
    } else {
        QLabel *valueWidget = new QLabel(value);
        valueWidget->setObjectName("settingValue");
        rowLayout->addWidget(valueWidget);
    }
    group->layout()->addWidget(row);
    if (lastRow) {
        row->setProperty("lastRow", true);
    }
}

// 更新WiFi网络列表
void SettingsPage::updateWifiList(const QVector<WifiNetwork> &networks)
{
    // 清空旧列表
    clearLayout(m_wifiListLayout);

    if (networks.isEmpty()) {
        QLabel *emptyLabel = new QLabel("未扫描到任何网络");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #9ca3af; padding: 20px;");
        m_wifiListLayout->addWidget(emptyLabel);
        return;
    }

    for (int i = 0; i < networks.size(); i++) {
        const WifiNetwork &net = networks[i];
        if (net.ssid.isEmpty()) continue;

        // 创建一行网络项
        QWidget *row = new QWidget;
        row->setObjectName("wifiRow");
        QString style;
        if (i != networks.size() - 1)
            style += "QWidget#wifiRow { border-bottom: 1px solid #e5e7eb; }";
        if (net.connected) {
            style += "QWidget#wifiRow { background-color: #f5f7fa; }";
        }
        row->setStyleSheet(style);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(16, 12, 16, 12);

        // 图标：信号强度
        QLabel *iconLabel = new QLabel;
        QString iconPath  = ":/images/images/WiFi强度-强.png";
        QPixmap pixmap(iconPath);
        iconLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLabel->setFixedSize(32, 32);

        // SSID 和连接状态
        QVBoxLayout *textLayout = new QVBoxLayout;
        QLabel *ssidLabel       = new QLabel(net.ssid);
        ssidLabel->setStyleSheet("font-size: 16px; font-weight: 500;");
        QLabel *statusLabel = new QLabel;
        if (net.connected) {
            statusLabel->setText("已连接");
            statusLabel->setStyleSheet("color: #10b981; font-size: 12px;");
        } else if (net.favorite) {
            statusLabel->setText("保存的网络");
            statusLabel->setStyleSheet("color: #6b7280; font-size: 12px;");
        } else {
            if (net.secured) {
                statusLabel->setText("加密，可连接");
            } else {
                statusLabel->setText("可直接连接");
            }
            statusLabel->setStyleSheet("color: #6b7280; font-size: 12px;");
        }
        textLayout->addWidget(ssidLabel);
        textLayout->addWidget(statusLabel);
        textLayout->setSpacing(2);

        // 忘记按钮（如果已保存）
        QPushButton *forgetBtn = nullptr;
        if (net.favorite) {
            forgetBtn = new QPushButton("忘记网络");
            forgetBtn->setFocusPolicy(Qt::NoFocus);
            forgetBtn->setProperty("ssid", net.ssid);
            forgetBtn->setProperty("secured", net.secured);
            connect(forgetBtn, &QPushButton::clicked, this, [this, ssid = net.ssid]() {
                m_wifiManager->forgetNetwork(ssid);
            });
        }

        // 连接按钮（如果未连接）
        QPushButton *actionBtn = nullptr;
        if (!net.connected) {
            actionBtn = new QPushButton("连接");
            actionBtn->setFocusPolicy(Qt::NoFocus);
            actionBtn->setProperty("ssid", net.ssid);
            actionBtn->setProperty("secured", net.secured);
            connect(actionBtn, &QPushButton::clicked, this, [this, ssid = net.ssid, secured = net.secured, favorite = net.favorite]() {
                onNetworkItemClicked(ssid, secured, favorite);
            });
        }

        rowLayout->addWidget(iconLabel);
        rowLayout->addLayout(textLayout, 1);
        if (forgetBtn) rowLayout->addWidget(forgetBtn);
        if (actionBtn) rowLayout->addWidget(actionBtn);

        m_wifiListLayout->addWidget(row);
    }
}

// 清空布局
void SettingsPage::clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

// --------------------- 槽函数 ---------------------
void SettingsPage::onWifiEnableToggle(bool checked)
{
    m_wifiEnableSwitch->blockSignals(true);

    if (checked) {
        m_wifiStatusLabel->setText("正在开启 WLAN...");
        m_wifiManager->enableWifi();
    } else {
        m_wifiStatusLabel->setText("正在关闭 WLAN...");
        m_wifiManager->disableWifi();
    }

    m_wifiEnableSwitch->blockSignals(false);
    m_wifiRefreshBtn->setEnabled(false);
}

void SettingsPage::onRefreshClicked()
{
    m_wifiStatusLabel->setText("正在扫描网络...");
    m_wifiRefreshBtn->setEnabled(false);
    m_wifiManager->scanNetworks();
}

void SettingsPage::onWifiEnabled()
{
    m_wifiStatusLabel->setText("WLAN 已开启，正在扫描...");
    m_wifiEnableSwitch->blockSignals(true);
    m_wifiEnableSwitch->setChecked(true);
    m_wifiEnableSwitch->blockSignals(false);
    m_wifiRefreshBtn->setEnabled(true);
    // 延迟 2 秒等硬件就绪后扫描
    QTimer::singleShot(2000, this, [this]() { m_wifiManager->scanNetworks(); });
}

void SettingsPage::onWifiDisabled()
{
    m_wifiStatusLabel->setText("WLAN 已关闭");
    m_wifiEnableSwitch->blockSignals(true);
    m_wifiEnableSwitch->setChecked(false);
    m_wifiEnableSwitch->blockSignals(false);
    m_wifiRefreshBtn->setEnabled(false);
    clearLayout(m_wifiListLayout);
    QLabel *disabledLabel = new QLabel("WLAN 未开启");
    disabledLabel->setAlignment(Qt::AlignCenter);
    disabledLabel->setStyleSheet("color: #9ca3af; padding: 20px;");
    m_wifiListLayout->addWidget(disabledLabel);
}

void SettingsPage::onWifiScanFinished(const QVector<WifiNetwork> &networks)
{
    m_wifiStatusLabel->setText(QString("扫描完成，发现 %1 个网络").arg(networks.size()));
    m_wifiRefreshBtn->setEnabled(true);
    updateWifiList(networks);
}

void SettingsPage::onWifiConnectionSuccess(const QString &ssid)
{
    m_wifiStatusLabel->setText(QString("已成功连接到 %1").arg(ssid));
    // 刷新列表以更新连接状态
    m_wifiManager->scanNetworks(); // 重新扫描获取最新状态
}

void SettingsPage::onWifiConnectionFailed(const QString &ssid, const QString &error)
{
    m_wifiStatusLabel->setText(QString("连接 %1 失败: %2").arg(ssid, error));
    // 可弹出对话框提示
}

void SettingsPage::onWifiDisconnected()
{
    m_wifiStatusLabel->setText("已断开 WLAN 连接");
    // 刷新列表
    m_wifiManager->scanNetworks();
}

void SettingsPage::onWifiForgotten(const QString &ssid)
{
    m_wifiStatusLabel->setText(QString("已忘记 %1").arg(ssid));
    // 刷新列表以更新忘记状态
    m_wifiManager->scanNetworks(); // 重新扫描获取最新状态
}

void SettingsPage::onWifiError(const QString &error)
{
    m_wifiStatusLabel->setText(QString("错误: %1").arg(error));
    // 重新查询实际状态并恢复开关显示
    bool actualState = m_wifiManager->isWifiEnabled();
    m_wifiEnableSwitch->blockSignals(true);
    m_wifiEnableSwitch->setChecked(actualState);
    m_wifiEnableSwitch->blockSignals(false);
    m_wifiRefreshBtn->setEnabled(actualState);
}

void SettingsPage::onNetworkItemClicked(const QString &ssid, bool secured, bool favorite)
{
    if (!secured) {
        m_wifiManager->connectNetwork(ssid);
    } else {
        if (favorite) {
            // 已保存的加密网络，直接连接（使用已存储的密码）
            m_wifiStatusLabel->setText(QString("正在连接已保存的网络 %1 ...").arg(ssid));
            m_wifiManager->connectNetwork(ssid); // 无密码参数，connman 会自动使用已保存的配置
        } else {
            bool ok;
            QString password;
            QInputDialog inputDialog(this);                 // 关键：指定父对象
            inputDialog.setWindowModality(Qt::WindowModal); // 关键：设置为窗口模态
            inputDialog.setInputMode(QInputDialog::TextInput);
            inputDialog.setTextEchoMode(QLineEdit::Password);
            inputDialog.setWindowTitle(QString("连接到 %1").arg(ssid));
            inputDialog.setLabelText(QString("输入 %1 的密码:").arg(ssid));
            inputDialog.setOkButtonText("确认");
            inputDialog.setCancelButtonText("取消");

            if (inputDialog.exec() == QDialog::Accepted) {
                password = inputDialog.textValue();
                if (!password.isEmpty()) {
                    m_wifiStatusLabel->setText(QString("正在连接 %1 ...").arg(ssid));
                    m_wifiManager->connectNetwork(ssid, password);
                }
            }
        }
    }
}