#include "vehicledataprocessor.h"
#include "config.h"
#include <QDebug>
#include <QTimer>
#include <QtMath>

VehicleDataProcessor *VehicleDataProcessor::instance()
{
    static VehicleDataProcessor instance; // C++11 线程安全，自动析构
    return &instance;
}

VehicleDataProcessor::VehicleDataProcessor(QObject *parent)
    : QObject(parent)
    , m_mockTimer(nullptr)
    , m_mockEnabled(false)
{
    // 连接真实 CAN 信号（模拟模式下仍可连接，但不会影响模拟数据）
    connect(CanManager::instance(), &CanManager::frameReceived,
            this, &VehicleDataProcessor::processCanFrame);
}

VehicleDataProcessor::~VehicleDataProcessor()
{
    if (m_mockTimer) {
        m_mockTimer->stop();
        delete m_mockTimer;
        m_mockTimer = nullptr;
    }
}

void VehicleDataProcessor::enableMockMode(bool enable)
{
    if (enable == m_mockEnabled)
        return;

    m_mockEnabled = enable;

    if (enable) {
        if (!m_mockTimer) {
            m_mockTimer = new QTimer(this);
            connect(m_mockTimer, &QTimer::timeout, this, &VehicleDataProcessor::mockSendData);
        }
        m_mockTimer->start(50); // 50ms 更新一次模拟数据
        qDebug() << "[VehicleDataProcessor] 模拟模式已开启";
    } else {
        if (m_mockTimer)
            m_mockTimer->stop();
        qDebug() << "[VehicleDataProcessor] 模拟模式已关闭";
    }
}

void VehicleDataProcessor::mockSendData()
{
    static int step = 0;
    step++;

    // 1. 档位：D → P → R 循环，每 100 步（约 5 秒）切换
    if (step % 100 == 0) {
        int gears[] = {Config::GEAR_D, Config::GEAR_P, Config::GEAR_R};
        int gear    = gears[(step / 100) % 3];
        emit gearChanged(gear);
        qDebug() << "[Mock] gearChanged:" << gear;
    }

    // 2. 超声波距离（uint16 cm）
    int dist = 150 - (step % 120);
    if (dist < 20) dist = 20;
    emit distanceChanged(dist);

    // 3. 动力域数据
    if (step % 10 == 0) {
        int rpm = 20 + (step % 80); // 转速指示 20-99%
        emit rpmChanged(rpm);
        // 转速百分比 → 车速：百分比*60→实际RPM，195/65R15 轮胎直径0.635m，传动比3.5
        int speed = static_cast<int>(rpm * 60 * 3.141592653589793 * 0.635 * 60.0 / (3.5 * 1000.0));
        emit speedChanged(speed);
        int fuel = 100 - (step % 70); // 油量 0-100%
        emit oilChanged(fuel);
    }

    // 4. 车身域数据（温度×10, 湿度×10）
    if (step % 10 == 0) {
        int tempX10 = 250 + (step % 100); // 25.0°C ~ 35.0°C
        emit temperatureChanged(tempX10);
        int humX10 = 500 + (step % 300); // 50.0% ~ 80.0%
        emit humidityChanged(humX10);
        int fan = 30 + (step % 60); // 风扇 0-100%
        emit fanSpeedChanged(fan);
        int win = 50 + (step % 40); // 车窗 0-100%
        emit windowPosChanged(win);
    }
}

void VehicleDataProcessor::processCanFrame(const CanManager::CanFrame &frame)
{
    if (m_mockEnabled)
        return;

    // 统一数据提取
    auto u8    = [&](int i) { return static_cast<quint8>(frame.data[i]); };
    auto s16le = [&](int i) { return static_cast<qint16>(u8(i) | (u8(i + 1) << 8)); };
    auto u16le = [&](int i) { return static_cast<quint16>(u8(i) | (u8(i + 1) << 8)); };

    switch (frame.id) {
        // ============ PT-CAN（节点A）上报 ============
        case Config::CAN_ID_A_STATUS: // 0x100 — 心跳+按键
            // data[0] = heartbeat counter (忽略)
            break;
        case Config::CAN_ID_A_ULTRASONIC: // 0x101 — 超声波距离cm
            if (frame.data.size() >= 2)
                emit distanceChanged(static_cast<int>(u16le(0)));
            break;
        case Config::CAN_ID_A_GEAR: // 0x102 — 档位 (0=P,1=R,2=N,3=D)
            if (frame.data.size() >= 1)
                emit gearChanged(static_cast<int>(u8(0)));
            break;
        case Config::CAN_ID_A_FUEL: // 0x103 — 油量×10
            if (frame.data.size() >= 2)
                emit oilChanged(static_cast<int>(u16le(0)));
            break;
        case Config::CAN_ID_A_RPM: { // 0x104 — 转速×10
            if (frame.data.size() >= 2) {
                int rpm = static_cast<int>(u16le(0));
                emit rpmChanged(rpm);
                // CAN 转速值=百分比×10 (0-1000)，→ 实际RPM=×6，再 → 车速
                int speed = static_cast<int>(rpm * 6 * 3.141592653589793 * 0.635 * 60.0 / (3.5 * 1000.0));
                emit speedChanged(speed);
            }
            break;
        }

        // ============ Body-CAN（节点B）上报 ============
        case Config::CAN_ID_B_STATUS: // 0x200 — 心跳+按键
            break;
        case Config::CAN_ID_B_TEMPERATURE: // 0x201 — 温度×10
            if (frame.data.size() >= 2)
                emit temperatureChanged(static_cast<int>(s16le(0)));
            break;
        case Config::CAN_ID_B_HUMIDITY: // 0x202 — 湿度×10
            if (frame.data.size() >= 2)
                emit humidityChanged(static_cast<int>(u16le(0)));
            break;
        default:
            break;
    }
}