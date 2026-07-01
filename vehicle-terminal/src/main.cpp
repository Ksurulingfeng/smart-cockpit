#include "mainwindow.h"
#include "canmanager.h"
#include "com_stack/pdur.h"
#include "com_stack/com.h"
#include "com_stack/cantp.h"
#include "com_stack/dcm.h"
#include "com_stack/dem.h"
#include "rte.h"
#include "config.h"
#include "ec20manager.h"
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <csignal>

// 原子标志，信号处理器中安全写入，主线程读取
static std::atomic<bool> s_shouldQuit{false};

static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        s_shouldQuit = true;
    }
}

int main(int argc, char *argv[])
{
#if __arm__
    qunsetenv("DISPLAY");
    qputenv("QT_IM_MODULE", "tgtsml");
    qputenv("XDG_RUNTIME_DIR", "/tmp/runtime-root");
#endif

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    QApplication app(argc, argv);

    // 定时检查退出信号，0ms 间隔 = 事件循环空闲即触发，无实质轮询开销
    QTimer quitChecker;
    QObject::connect(&quitChecker, &QTimer::timeout, [&]() {
        if (s_shouldQuit) {
            qDebug() << "[INFO] 收到退出信号";
            app.quit();
        }
    });
    quitChecker.start(0);

    // 初始化 CAN Stack: CanMgr → PduR(路由) → Com(E2E+解包) / CanTp(多帧) → Rte
    CanManager::instance()->connectDevice(Config::CAN_DEVICE, CAN_BITRATE_HZ);
    CanTp::instance()->configure();   // UDS 多帧传输
    PduR::instance();                 // 建立 CanManager ↔ Com/CanTp 路由
    Com::instance();                  // E2E 校验 + 信号解包
    Rte::instance()->setMode(Rte::REAL_CAN);

    // 诊断栈: CanTp → DCM → DEM
    Dem::instance();                  // DTC 故障管理（从 NVM 加载）
    Dcm::instance();                  // UDS 服务处理
    QObject::connect(CanTp::instance(), &CanTp::messageReceived,
                     Dcm::instance(), &Dcm::processUdsMessage);
    QObject::connect(Dcm::instance(), &Dcm::sendUdsResponse,
                     CanTp::instance(), &CanTp::sendMessage);

    // E2E 校验错误 → DEM 故障上报
    QObject::connect(Com::instance(), &Com::e2eError,
                     Dem::instance(), [](uint32_t canId, const QString &reason) {
                         Q_UNUSED(canId)
                         Q_UNUSED(reason)
                         Dem::instance()->reportEvent(DTC_E2E_ERROR,
                             DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED_DTC);
                     });

    // EC20 4G 模块
    EC20Manager::instance()->init();

    MainWindow mainWindow;
    mainWindow.show();
    qDebug() << "[OK] Qt界面已启动";

    return app.exec();
}
