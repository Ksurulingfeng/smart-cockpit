#include "rte.h"
#include "comlayer.h"
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

Rte::Rte()
{
    connect(CanManager::instance(), &CanManager::frameReceived,
            this, [this](const CanManager::CanFrame &frame) {
                if (m_mode == REAL_CAN)
                    onCanFrame(frame.id, (const uint8_t *)frame.data.constData(), frame.data.size());
            });

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
    ComLayer::instance()->resetCounters();
    m_mockStep = 0;
    if (m == MOCK) m_mockTimer->start(50);
    else           m_mockTimer->stop();
}

static int rpmToSpeed(int rpmX10) {
    return (int)((rpmX10 / 10) * 60 * 3.141592653589793 * 0.635 * 60.0 / (3.5 * 1000.0));
}

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

void Rte::onCanFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    ComLayer::instance()->processCanFrame(canId, data, len);

    if (canId >= 0x180 && canId <= 0x183) m_nodeTimerA.restart();
    if (canId >= 0x200 && canId <= 0x203) m_nodeTimerB.restart();

    if (len < 2) return;
    auto u8    = [&](int i) -> quint8  { return data[i]; };
    auto s16le = [&](int i) -> qint16  { return (qint16)(u8(i) | (u8(i + 1) << 8)); };
    auto u16le = [&](int i) -> quint16 { return (quint16)(u8(i) | (u8(i + 1) << 8)); };
    bool valid = CAN_GET_VALID(u8(0));

    switch (canId) {
        case CAN_ID_A_ULTRASONIC:
            if (valid) emit distanceChanged((int)u16le(1));
            break;
        case CAN_ID_A_GEAR:
            if (valid) emit gearChanged((int)u8(1));
            break;
        case CAN_ID_A_FUEL:
            if (valid) emit oilChanged((int)u16le(1));
            break;
        case CAN_ID_A_RPM: {
            if (valid) {
                int rpm = (int)u16le(1);
                emit rpmChanged(rpm);
                emit speedChanged(rpmToSpeed(rpm));
            }
            break;
        }
        case CAN_ID_B_TEMPERATURE:
            if (valid) emit temperatureChanged((int)s16le(1));
            break;
        case CAN_ID_B_HUMIDITY:
            if (valid) emit humidityChanged((int)u16le(1));
            break;
        case CAN_ID_B_FAN_SPEED_ACTUAL:
            if (valid) emit fanSpeedChanged((int)u8(1));
            break;
        case CAN_ID_B_WINDOW_POS_ACTUAL:
            if (valid) emit windowPosChanged((int)u8(1));
            break;
    }
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
