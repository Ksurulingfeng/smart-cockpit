#include "mainwindow.h"
#include "canmanager.h"
#include "vehicledataprocessor.h"
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

    // 初始化 CAN 和 EC20 4G 模块
    CanManager::instance()->connectDevice(Config::CAN_DEVICE, Config::CAN_BITRATE);
    VehicleDataProcessor::instance();
    EC20Manager::instance()->init();

    MainWindow mainWindow;
    mainWindow.show();
    qDebug() << "[OK] Qt界面已启动";

    return app.exec();
}
