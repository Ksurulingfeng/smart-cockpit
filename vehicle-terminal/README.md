### 摄像头测试
```bash
#include "RearViewCamera.h"
#include "v4l2camera.h"
#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    RearViewCamera rearCam;
    rearCam.start("/dev/video1", 640, 480);

    // 模拟距离数据（实际应连接 CAN 解析信号）
    QTimer distTimer;
    int dist = 150;
    QObject::connect(&distTimer, &QTimer::timeout, [&]() {
        dist = (dist - 10) % 200;
        if (dist < 30) dist = 150;
        rearCam.setDistance(dist);
    });
    distTimer.start(200);

    return a.exec();
}
```