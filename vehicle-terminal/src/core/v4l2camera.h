#ifndef V4L2CAMERA_H
#define V4L2CAMERA_H

#include <QObject>
#include <QImage>
#include <QTimer>
#include <vector>

class V4l2Camera : public QObject
{
    Q_OBJECT
public:
    explicit V4l2Camera(QObject *parent = nullptr);
    ~V4l2Camera() override;

    // 打开摄像头设备，width/height 为期望分辨率（OV5640 一般支持 640x480）
    bool open(const char *device, int width = 640, int height = 480);
    void close();
    bool start(); // 开始采集
    void stop();  // 停止采集

signals:
    void imageReady(const QImage &image); // 每帧信号

private slots:
    void readFrame(); // 定时器槽，读取一帧

private:
    QImage rgb565ToQImage(const void *data, int width, int height);

    int fd;                                  // 设备文件描述符
    int width, height;                       // 图像宽高
    std::vector<void *> buffers;             // mmap 缓冲区指针
    unsigned int n_buffers;                  // 缓冲区个数
    std::vector<unsigned int> bufferLengths; // 缓冲区长度向量
    QTimer *timer;                           // 定时器，控制帧率
};

#endif // V4L2CAMERA_H