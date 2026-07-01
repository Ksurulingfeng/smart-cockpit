/**
 * @file    pdur.cpp
 * @brief   PduR 实现 — CAN 帧路由分发
 *
 * 接收路由:
 *   传感器帧 (0x180-0x27F) → Com::receiveFrame
 *   诊断帧   (0x300-0x37F) → Com::receiveFrame
 *   UDS 帧   (0x7E0-0x7EF) → CanTp::receiveFrame (Phase 3)
 *
 * 发送路由:
 *   Com::sendFrameRequest    → CanManager::sendFrame
 *   CanTp::sendFrameRequest  → CanManager::sendFrame (Phase 3)
 */

#include "pdur.h"
#include "com_stack/com.h"
#include "com_stack/cantp.h"
#include "canmanager.h"
#include "config.h"
#include <QDebug>

// ==================== 单例 ====================

PduR *PduR::instance()
{
    static PduR inst;
    return &inst;
}

PduR::PduR(QObject *parent)
    : QObject(parent)
{
    setupRoutes();
}

// ==================== 路由连接 ====================

void PduR::setupRoutes()
{
    CanManager *canMgr = CanManager::instance();

    // 接收路由：CanManager → 本 PduR → 上层模块
    connect(canMgr, &CanManager::frameReceived, this,
            [this](const CanManager::CanFrame &frame) {
                onCanFrame(frame.id,
                           (const uint8_t *)frame.data.constData(),
                           frame.data.size());
            });

    // 发送路由：Com → CanManager
    connect(Com::instance(), &Com::sendFrameRequest, this,
            [this](uint32_t canId, const QByteArray &data) {
                onSendFrameRequest(canId, data);
            });

    // 发送路由：CanTp → CanManager
    connect(CanTp::instance(), &CanTp::sendFrameRequest, this,
            [this](uint32_t canId, const QByteArray &data) {
                onSendFrameRequest(canId, data);
            });
}

// ==================== 接收分发 ====================

void PduR::onCanFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    // 传感器帧 0x180-0x27F → Com
    if (canId >= 0x180 && canId <= 0x27F) {
        Com::instance()->receiveFrame(canId, data, len);
        return;
    }

    // 诊断帧 0x300-0x37F → Com
    if (canId >= 0x300 && canId <= 0x37F) {
        Com::instance()->receiveFrame(canId, data, len);
        return;
    }

    // UDS 帧 0x7E0-0x7EF → CanTp
    if (canId >= 0x7E0 && canId <= 0x7EF) {
        CanTp::instance()->receiveFrame(canId, data, len);
        return;
    }
}

// ==================== 发送转发 ====================

void PduR::onSendFrameRequest(uint32_t canId, const QByteArray &data)
{
    CanManager::instance()->sendFrame(canId, data);
}
