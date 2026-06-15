#ifndef VEHICLEDATAPROCESSOR_H
#define VEHICLEDATAPROCESSOR_H

#include <QObject>
#include "canmanager.h"

class QTimer;

class VehicleDataProcessor : public QObject
{
    Q_OBJECT
public:
    // 禁止拷贝和赋值
    VehicleDataProcessor(const VehicleDataProcessor &)            = delete;
    VehicleDataProcessor &operator=(const VehicleDataProcessor &) = delete;

    // 全局访问点（线程安全）
    static VehicleDataProcessor *instance();

    // 启用模拟模式（仅测试用，会替代真实 CAN 数据）
    void enableMockMode(bool enable);

signals:
    // PT-CAN（节点A）上报
    void speedChanged(int kmh);
    void rpmChanged(int rpm);
    void oilChanged(int percent);
    void gearChanged(int gear);
    void distanceChanged(int cm);
    // Body-CAN（节点B）上报
    void temperatureChanged(int celsiusX10);
    void humidityChanged(int percentX10);
    void fanSpeedChanged(int percent);
    void windowPosChanged(int percent);

public slots:
    void processCanFrame(const CanManager::CanFrame &frame);

private:
    explicit VehicleDataProcessor(QObject *parent = nullptr);
    ~VehicleDataProcessor() override;

    void mockSendData(); // 模拟数据发送（由定时器调用）

    QTimer *m_mockTimer;
    bool m_mockEnabled;
};

#endif // VEHICLEDATAPROCESSOR_H