/**
 * @file    com.h
 * @brief   AUTOSAR Com 层 — E2E 校验 + 统一信号解包/打包
 *
 * 替代旧 ComLayer，增加：
 * - QObject 信号机制（signalUpdated 统一通知 Rte）
 * - E2E 校验（valid 标志 + 滚动计数器）
 * - 诊断帧 (0x300) 解析
 * - 发送计数器管理
 */

#ifndef COM_H
#define COM_H

#include <QObject>
#include <cstdint>
#include "com_signal_defs.h"

class Com : public QObject
{
    Q_OBJECT
public:
    static Com *instance();
    Com(const Com &) = delete;
    Com &operator=(const Com &) = delete;

    // ---- 接收（由 PduR 调用） ----
    void receiveFrame(uint32_t canId, const uint8_t *data, uint8_t len);

    // ---- 发送（向后兼容 ComLayer::sendSignal） ----
    void sendSignal(SignalId_t id, uint32_t value);

    // ---- E2E 管理 ----
    void resetCounters();

    // ---- 信号缓存查询（DCM 通过 DID 读取） ----
    uint32_t getSignalValue(SignalId_t id) const;

signals:
    // 核心：统一信号更新 → Rte 路由
    void signalUpdated(SignalId_t id, uint32_t value);

    // 诊断帧数据 (0x300)
    void diagnosticDataReceived(uint16_t tec, uint16_t rec,
                                uint16_t txCount, uint8_t busLoad);

    // E2E 错误通知
    void e2eError(uint32_t canId, const QString &reason);

    // 发送帧请求 → PduR → CanManager
    void sendFrameRequest(uint32_t canId, const QByteArray &data);

private:
    explicit Com(QObject *parent = nullptr);
    ~Com() override = default;

    // E2E 校验一帧
    bool validateE2E(uint32_t canId, const uint8_t *data, uint8_t len,
                     const SignalDesc_t *flagsDesc);
    // 解包该 canId 下所有数据信号并发射 signalUpdated
    void unpackAndEmit(uint32_t canId, const uint8_t *data);

    // 接收侧计数器
    uint8_t m_lastCounter[0x400] = {};
    bool    m_counterInit[0x400] = {};

    // 发送侧计数器（控制帧也需要维护 rolling counter）
    uint8_t m_sendCounter[0x400] = {};

    // 信号值缓存
    uint32_t m_signalCache[SID_COUNT] = {};
};

#endif // COM_H
