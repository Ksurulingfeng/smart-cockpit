#ifndef REARVIEWCAMERA_H
#define REARVIEWCAMERA_H

#include <QWidget>
#include <QImage>
#include <QTimer>
#include "v4l2camera.h"

class RearViewCamera : public QWidget
{
    Q_OBJECT
public:
    explicit RearViewCamera(QWidget *parent = nullptr);
    ~RearViewCamera() override;

public slots:
    void onGearChanged(int gear);   // 档位变化信号处理
    void onDistanceChanged(int cm); // 距离变化信号处理

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onImageReady(const QImage &image);
    void onAlertTimeout();

private:
    void start(const QString &device, int width, int height); // 启动摄像头
    void stop();                                              // 停止摄像头
    void setDistance(int cm);                                 // 设置距离
    QImage scaleToFit(const QImage &image);                   // 缩放图像以适应窗口大小

    V4l2Camera *m_camera;
    QImage m_currentFrame;
    int m_distance = -1; // 单位 cm, -1 表示无效/不显示
    bool m_active   = false;
    bool m_alertActive    = false;
    bool m_alertSuppressed = false;
    QTimer *m_alertTimer  = nullptr;
};

#endif // REARVIEWCAMERA_H