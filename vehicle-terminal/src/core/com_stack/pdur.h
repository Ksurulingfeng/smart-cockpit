/**
 * @file    pdur.h
 * @brief   AUTOSAR PduR — PDU Router，CAN 帧按 ID 范围路由到上层模块
 */

#ifndef PDUR_H
#define PDUR_H

#include <QObject>
#include <QByteArray>
#include <cstdint>

class PduR : public QObject
{
    Q_OBJECT
public:
    static PduR *instance();
    PduR(const PduR &) = delete;
    PduR &operator=(const PduR &) = delete;

private slots:
    void onCanFrame(uint32_t canId, const uint8_t *data, uint8_t len);
    void onSendFrameRequest(uint32_t canId, const QByteArray &data);

private:
    explicit PduR(QObject *parent = nullptr);
    ~PduR() override = default;

    // 在构造中连接 CanManager::frameReceived 和 Com/CanTp 的发送请求
    void setupRoutes();
};

#endif // PDUR_H
