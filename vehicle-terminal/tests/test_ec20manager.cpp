#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QDir>
#include "ec20manager.h"

static int s_total = 0;
static int s_passed = 0;

static void check(const QString &name, bool ok)
{
    s_total++;
    if (ok) {
        s_passed++;
        qDebug().noquote() << QString("  [PASS] %1").arg(name);
    } else {
        qDebug().noquote() << QString("  [FAIL] %1").arg(name);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    bool mockMode = app.arguments().contains("--mock");

    if (mockMode) {
        qDebug() << "========== EC20Manager Mock GPS 测试 ==========\n";

        EC20Manager *ec20 = EC20Manager::instance();

        QObject::connect(ec20, &EC20Manager::positionUpdated,
                         [](double lon, double lat) {
                             qDebug().noquote() << QString("  [POS] %1, %2")
                                                       .arg(lat, 0, 'f', 6)
                                                       .arg(lon, 0, 'f', 6);
                         });
        QObject::connect(ec20, &EC20Manager::gpsStatusChanged,
                         [](bool fixed) {
                             qDebug().noquote() << "  [GPS]" << (fixed ? "已定位" : "丢失定位");
                         });
        QObject::connect(ec20, &EC20Manager::errorOccurred,
                         [](const QString &err) { qDebug().noquote() << "  [ERR ]" << err; });

        // 查找 NMEA 文件
        QString nmeaPath = QDir(QCoreApplication::applicationDirPath())
                               .filePath("../tests/mock_route.nmea");
        if (!QFile::exists(nmeaPath))
            nmeaPath = "tests/mock_route.nmea";
        if (!QFile::exists(nmeaPath))
            nmeaPath = "mock_route.nmea";

        qDebug() << "NMEA 文件:" << nmeaPath;
        bool loaded = ec20->loadNmeaFile(nmeaPath);
        check("loadNmeaFile()", loaded);
        if (!loaded) {
            qDebug() << "[ABORT] 找不到 mock_route.nmea";
            return 1;
        }

        bool started = ec20->startMockGps(200);
        check("startMockGps()", started);

        qDebug() << "\n回放中，60 秒后自动退出...\n";

        // 5 秒后检查位置更新计数
        int posCount = 0;
        QObject::connect(ec20, &EC20Manager::positionUpdated,
                         [&](double, double) { posCount++; });
        QTimer::singleShot(5000, [&]() {
            qDebug().noquote() << QString("  5秒内收到 %1 次位置更新").arg(posCount);
            check("位置更新频率正常 (>=10)", posCount >= 10);
        });

        QTimer::singleShot(60000, [&]() {
            ec20->stopMockGps();
            qDebug() << "\n========== 测试结果 ==========";
            qDebug().noquote() << QString("通过: %1 / %2").arg(s_passed).arg(s_total);
            QCoreApplication::quit();
        });

        return app.exec();
    }

    // ==================== 真实硬件测试 ====================

    EC20Manager *ec20 = EC20Manager::instance();

    QObject::connect(ec20, &EC20Manager::errorOccurred,
                     [](const QString &err) { qDebug().noquote() << "  [ERR ]" << err; });

    qDebug() << "========== EC20Manager 测试开始 ==========\n";

    // 1. 初始化
    qDebug() << "--- 1. 初始化模块 ---";
    bool initOk = ec20->init();
    check("init()", initOk);
    if (!initOk) {
        qDebug() << "\n[ABORT] 初始化失败，测试终止（请检查 EC20 硬件连接和 ttyUSB2 端口）";
        return 1;
    }

    // 2. AT 通信
    qDebug() << "\n--- 2. AT 通信测试 ---";
    check("isOnline()", ec20->isOnline());

    // 3. SIM 卡状态
    qDebug() << "\n--- 3. SIM 卡 ---";
    EC20Manager::SimStatus sim = ec20->getSimStatus();
    QString simStr;
    switch (sim) {
        case EC20Manager::SimReady:       simStr = "就绪"; break;
        case EC20Manager::SimPinRequired: simStr = "需要 PIN 码"; break;
        case EC20Manager::SimPukRequired: simStr = "需要 PUK 码"; break;
        case EC20Manager::SimNotInserted: simStr = "未插入"; break;
        default:                          simStr = "未知"; break;
    }
    qDebug().noquote() << "  SIM 状态:" << simStr;
    check("getSimStatus() == SimReady", sim == EC20Manager::SimReady);

    // 4. 信号质量
    qDebug() << "\n--- 4. 信号质量 ---";
    int rssi = 0, ber = 0;
    bool sigOk = ec20->getSignalQuality(rssi, ber);
    check("getSignalQuality()", sigOk);
    if (sigOk) {
        qDebug().noquote() << QString("  RSSI: %1, BER: %2").arg(rssi).arg(ber);
    }

    // 5. 网络注册
    qDebug() << "\n--- 5. 网络注册 ---";
    EC20Manager::NetworkStatus net = ec20->getNetworkRegistration();
    QString netStr;
    switch (net) {
        case EC20Manager::NetRegisteredHome:    netStr = "已注册（归属网络）"; break;
        case EC20Manager::NetRegisteredRoaming: netStr = "已注册（漫游）"; break;
        case EC20Manager::NetSearching:         netStr = "搜索中"; break;
        case EC20Manager::NetDenied:            netStr = "被拒绝"; break;
        case EC20Manager::NetNotRegistered:     netStr = "未注册"; break;
        default:                                netStr = "未知"; break;
    }
    qDebug().noquote() << "  网络状态:" << netStr;
    check("已注册", net == EC20Manager::NetRegisteredHome ||
                   net == EC20Manager::NetRegisteredRoaming);

    // 6. 模块信息
    qDebug() << "\n--- 6. 模块信息 ---";
    QString mf, model, rev;
    bool infoOk = ec20->getModuleInfo(mf, model, rev);
    check("getModuleInfo()", infoOk);
    if (infoOk) {
        qDebug().noquote() << "  厂商:" << mf;
        qDebug().noquote() << "  型号:" << model;
        qDebug().noquote() << "  版本:" << rev;
    }

    // 7. IMEI / ICCID
    qDebug() << "\n--- 7. IMEI / ICCID ---";
    QString imei = ec20->getImei();
    bool imeiOk = !imei.isEmpty() && imei.length() == 15;
    check("IMEI (15位)", imeiOk);
    qDebug().noquote() << "  IMEI:" << imei;

    QString iccid = ec20->getIccid();
    bool iccidOk = !iccid.isEmpty();
    check("ICCID", iccidOk);
    qDebug().noquote() << "  ICCID:" << iccid;

    // 8. GPS
    qDebug() << "\n--- 8. GPS 定位 ---";
    bool gpsOk = ec20->startGps();
    check("startGps()", gpsOk);
    if (gpsOk) {
        qDebug() << "  等待 GPS 定位（最多 60 秒）...";
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);

        QObject::connect(ec20, &EC20Manager::positionUpdated,
                         [](double lon, double lat) {
                             qDebug().noquote() << QString("  位置: %1, %2")
                                                       .arg(lat, 0, 'f', 6)
                                                       .arg(lon, 0, 'f', 6);
                         });
        QObject::connect(ec20, &EC20Manager::gpsStatusChanged,
                         [](bool fixed) {
                             qDebug().noquote() << "  GPS 定位:" << (fixed ? "已定位" : "丢失定位");
                         });

        QTimer::singleShot(30000, [&]() {
            double lon = 0, lat = 0;
            bool locOk = ec20->queryLocationOnce(lon, lat);
            check("queryLocationOnce()", locOk);
            if (locOk)
                qDebug().noquote() << QString("  单次查询: lat=%1, lon=%2")
                                                       .arg(lat, 0, 'f', 6)
                                                       .arg(lon, 0, 'f', 6);
        });

        QTimer::singleShot(62000, [&]() {
            ec20->stopGps();
            check("stopGps()", true);
        });
    }

    // 9. SMS
    qDebug() << "\n--- 9. SMS 短信功能 ---";
    bool smsModeOk = ec20->setSmsTextMode(true);
    check("setSmsTextMode()", smsModeOk);

    QVector<SmsEntry> msgs = ec20->listMessages("ALL");
    qDebug().noquote() << QString("  已存储短信: %1 条").arg(msgs.size());
    for (const SmsEntry &e : msgs) {
        qDebug().noquote() << QString("    [%1] %2 %3").arg(e.index).arg(e.sender, e.timestamp);
        if (!e.content.isEmpty())
            qDebug().noquote() << "         " << e.content;
    }

    qDebug() << "\n========== 测试结果 ==========";
    qDebug().noquote() << QString("通过: %1 / %2").arg(s_passed).arg(s_total);

    if (!gpsOk) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
