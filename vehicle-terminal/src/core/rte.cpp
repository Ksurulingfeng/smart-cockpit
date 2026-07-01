/**
 * @file    rte.cpp
 * @brief   Rte 运行时环境 — 信号路由 + MOCK 模式 + 心跳检测
 *
 * 改造后：
 * - 删除了 onCanFrame 中的手动字节解包（迁移到 Com 层）
 * - onSignalUpdated 纯路由：SignalId_t → typed Qt signals
 * - 心跳检测仅刷新计时器，不解析数据
 */

#include "rte.h"
#include "com_stack/com.h"
#include "canmanager.h"
#include "config.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

Rte *Rte::instance()
{
    static Rte inst;
    return &inst;
}

Rte::Rte(QObject *parent)
    : QObject(parent)
{
    // REAL_CAN 模式：仅刷新心跳计时器（解包/校验由 Com 层负责）
    connect(CanManager::instance(), &CanManager::frameReceived,
            this, [this](const CanManager::CanFrame &frame) {
                if (m_mode == REAL_CAN)
                    refreshNodeTimer(frame.id);
            });

    // 订阅 Com 层统一解包后的信号更新
    connect(Com::instance(), &Com::signalUpdated,
            this, &Rte::onSignalUpdated);

    m_mockTimer = new QTimer(this);
    connect(m_mockTimer, &QTimer::timeout, this, &Rte::mockTick);
    m_mockTimer->start(50); // 默认 MOCK 模式

    m_nodeTimer = new QTimer(this);
    connect(m_nodeTimer, &QTimer::timeout, this, &Rte::checkNodes);
    m_nodeTimer->start(1000);
    m_nodeTimerA.start();
    m_nodeTimerB.start();
}

void Rte::setMode(Mode m)
{
    if (m == m_mode) return;
    m_mode = m;
    Com::instance()->resetCounters();
    m_mockStep = 0;
    if (m == MOCK) m_mockTimer->start(50);
    else           m_mockTimer->stop();
}

int Rte::rpmToSpeed(int rpmX10)
{
    return (int)((rpmX10 / 10) * 60 * 3.141592653589793 * 0.635 * 60.0 / (3.5 * 1000.0));
}

// ==================== MOCK 模式（完全不变） ====================

void Rte::mockTick()
{
    if (m_mode != MOCK) return;
    m_mockStep++;
    if (m_mockStep % 100 == 0)
        m_mockGear = (m_mockGear == CAN_GEAR_P) ? CAN_GEAR_R :
                      (m_mockGear == CAN_GEAR_R) ? CAN_GEAR_D : CAN_GEAR_P;

    int dist = 150 - (m_mockStep % 120);
    if (dist < 20) dist = 20;
    emit distanceChanged(dist);

    if (m_mockStep % 10 == 0) {
        int rpm  = (20 + (m_mockStep % 80)) * 10;
        int fuel = (100 - (m_mockStep % 70)) * 10;
        int temp = 250 + (m_mockStep % 100);
        int hum  = 500 + (m_mockStep % 300);
        int fan  = 30 + (m_mockStep % 60);
        int win  = 50 + (m_mockStep % 40);
        int speed = 0;
        if (m_mockGear == CAN_GEAR_D)      speed = rpmToSpeed(rpm);
        else if (m_mockGear == CAN_GEAR_R) speed = rpmToSpeed(rpm) / 4;

        emit rpmChanged(rpm);
        emit speedChanged(speed);
        emit oilChanged(fuel);
        emit temperatureChanged(temp);
        emit humidityChanged(hum);
        emit fanSpeedChanged(fan);
        emit windowPosChanged(win);
    }
    emit gearChanged(m_mockGear);
}

// ==================== Com→Rte 信号路由（替代旧 onCanFrame 解包） ====================

void Rte::onSignalUpdated(SignalId_t id, uint32_t value)
{
    if (m_mode != REAL_CAN) return; // MOCK 模式由 mockTick 独立驱动

    switch (id) {
    case SID_DISTANCE_CM:
        emit distanceChanged((int)value);
        break;
    case SID_GEAR:
        emit gearChanged((int)value);
        break;
    case SID_FUEL_PERCENT_X10:
        emit oilChanged((int)value);
        break;
    case SID_RPM_PERCENT_X10: {
        int rpm = (int)value;
        emit rpmChanged(rpm);
        emit speedChanged(rpmToSpeed(rpm));
        break;
    }
    case SID_TEMPERATURE_X10:
        emit temperatureChanged((int)(int16_t)value);
        break;
    case SID_HUMIDITY_X10:
        emit humidityChanged((int)value);
        break;
    case SID_FAN_ACTUAL_SPEED:
        emit fanSpeedChanged((int)value);
        break;
    case SID_WINDOW_ACTUAL_POS:
        emit windowPosChanged((int)value);
        break;
    default:
        break;
    }
}

// ==================== 心跳检测 ====================

void Rte::refreshNodeTimer(uint32_t canId)
{
    if (canId >= 0x180 && canId <= 0x183) m_nodeTimerA.restart();
    if (canId >= 0x200 && canId <= 0x203) m_nodeTimerB.restart();
}

void Rte::checkNodes()
{
    auto check = [this](int nodeId, QElapsedTimer &t, bool &wasOnline) {
        bool online = t.elapsed() < CAN_HEARTBEAT_TIMEOUT_MS;
        if (online && !wasOnline)      emit nodeOnline(nodeId);
        else if (!online && wasOnline) emit nodeOffline(nodeId);
        wasOnline = online;
    };
    check(CAN_NODE_A_ID, m_nodeTimerA, m_nodeAOnline);
    check(CAN_NODE_B_ID, m_nodeTimerB, m_nodeBOnline);
}

bool Rte::isNodeOnline(int nodeId) const
{
    return (nodeId == 0) ? m_nodeAOnline : m_nodeBOnline;
}
