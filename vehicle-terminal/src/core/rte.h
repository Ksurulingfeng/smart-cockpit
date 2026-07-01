#ifndef RTE_H
#define RTE_H

#include <QObject>
#include <QElapsedTimer>
#include "com_signal_defs.h"

class QTimer;

class Rte : public QObject
{
    Q_OBJECT
public:
    enum Mode { REAL_CAN, MOCK };

    static Rte *instance();
    Rte(const Rte &) = delete;
    Rte &operator=(const Rte &) = delete;

    void setMode(Mode m);
    Mode mode() const { return m_mode; }

signals:
    void speedChanged(int kmh);
    void rpmChanged(int rpmX10);
    void oilChanged(int percentX10);
    void gearChanged(int gear);
    void distanceChanged(int cm);
    void temperatureChanged(int celsiusX10);
    void humidityChanged(int percentX10);
    void fanSpeedChanged(int percent);
    void windowPosChanged(int percent);
    void nodeOnline(int nodeId);
    void nodeOffline(int nodeId);

private slots:
    void mockTick();

private:
    explicit Rte(QObject *parent = nullptr);
    ~Rte() override = default;

    void onCanFrame(uint32_t canId, const uint8_t *data, uint8_t len);
    void checkNodes();

    Mode m_mode = MOCK;
    QTimer *m_mockTimer = nullptr;
    QTimer *m_nodeTimer = nullptr;
    int m_mockStep = 0;
    int m_mockGear = CAN_GEAR_P;
    QElapsedTimer m_nodeTimerA;
    QElapsedTimer m_nodeTimerB;
    bool m_nodeAOnline = false;
    bool m_nodeBOnline = false;
};

#endif // RTE_H
