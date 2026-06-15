#include "v4l2camera.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <QDebug>

V4l2Camera::V4l2Camera(QObject *parent)
    : QObject(parent)
    , fd(-1)
    , width(640)
    , height(480)
    , n_buffers(0)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &V4l2Camera::readFrame);
}

V4l2Camera::~V4l2Camera()
{
    close();
}

bool V4l2Camera::open(const char *device, int w, int h)
{
    if (fd >= 0) close();

    width  = w;
    height = h;

    fd     = ::open(device, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
        qDebug() << "Failed to open" << device;
        return false;
    }

    // 设置格式为 RGB565
    struct v4l2_format fmt  = {};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        qDebug() << "Failed to set RGB565 format";
        close();
        return false;
    }

    // 申请缓冲区
    struct v4l2_requestbuffers req = {};
    req.count                      = 2;
    req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory                     = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        qDebug() << "Failed to request buffers";
        close();
        return false;
    }
    n_buffers = req.count;
    bufferLengths.resize(n_buffers);
    buffers.resize(n_buffers, nullptr);

    // mmap 映射
    for (unsigned int i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        buf.index              = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            qDebug() << "Failed to query buffer" << i;
            close();
            return false;
        }
        bufferLengths[i] = buf.length;
        buffers[i]       = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buf.m.offset);
        if (buffers[i] == MAP_FAILED) {
            qDebug() << "Failed to mmap buffer" << i;
            close();
            return false;
        }
    }

    // 入队所有缓冲区
    for (unsigned int i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        buf.index              = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "Failed to queue buffer" << i;
            close();
            return false;
        }
    }

    return true;
}

void V4l2Camera::close()
{
    if (fd >= 0) {
        stop(); // 确保流已停止

        // 释放 mmap 映射（使用保存的正确长度）
        for (unsigned int i = 0; i < n_buffers && i < buffers.size(); ++i) {
            if (buffers[i] && i < bufferLengths.size()) {
                munmap(buffers[i], bufferLengths[i]);
                buffers[i] = nullptr;
            }
        }
        buffers.clear();
        bufferLengths.clear();
        n_buffers = 0;

        ::close(fd);
        fd = -1;
    }
}

bool V4l2Camera::start()
{
    if (fd < 0) return false;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        qDebug() << "Failed to start stream";
        return false;
    }

    timer->start(33);
    return true;
}

void V4l2Camera::stop()
{
    timer->stop();
    if (fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
}

void V4l2Camera::readFrame()
{
    if (fd < 0) return;

    struct v4l2_buffer buf = {};
    buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory             = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        return; // 无帧可用
    }

    if (buf.index < n_buffers) {
        QImage img = rgb565ToQImage(buffers[buf.index], width, height);
        emit imageReady(img);
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        qDebug() << "Failed to re-queue buffer" << buf.index;
    }
}

QImage V4l2Camera::rgb565ToQImage(const void *data, int w, int h)
{
    const unsigned short *src = (const unsigned short *)data;
    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        uchar *line = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            unsigned short pixel = src[y * w + x];
            int r                = (pixel >> 11) & 0x1F;
            int g                = (pixel >> 5) & 0x3F;
            int b                = pixel & 0x1F;
            line[x * 3 + 0]      = (r * 255 + 15) / 31;
            line[x * 3 + 1]      = (g * 255 + 31) / 63;
            line[x * 3 + 2]      = (b * 255 + 15) / 31;
        }
    }
    return img;
}