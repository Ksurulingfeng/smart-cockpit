/**
 * @file    dem.h
 * @brief   AUTOSAR DEM — 故障事件管理
 *
 * 负责 DTC 的存储、上报、清除，以及 MIL (故障灯) 状态管理。
 */

#ifndef DEM_H
#define DEM_H

#include <QObject>
#include <QVector>
#include <cstdint>

// DTC 状态位 (AUTOSAR UDS status byte)
#define DTC_STATUS_TEST_FAILED            (1 << 0)
#define DTC_STATUS_TEST_FAILED_THIS_CYCLE (1 << 1)
#define DTC_STATUS_PENDING_DTC            (1 << 2)
#define DTC_STATUS_CONFIRMED_DTC          (1 << 3)
#define DTC_STATUS_TEST_FAILED_SINCE_CLR  (1 << 5)

// DTC 条目
struct DtcEntry
{
    uint32_t dtc;           // 3 字节 DTC 码
    uint8_t  statusMask;    // bit0=testFailed, bit3=confirmed, bit5=testFailedSinceClear ...
    uint32_t occurrenceCount;
};

class Dem : public QObject
{
    Q_OBJECT
public:
    static Dem *instance();
    Dem(const Dem &) = delete;
    Dem &operator=(const Dem &) = delete;

    // 故障事件上报
    void reportEvent(uint32_t dtcCode, uint8_t status);

    // DTC 管理
    void clearAllDtcs();
    QVector<DtcEntry> dtcList() const;
    int dtcCount() const;

    // MIL 状态
    bool isMilOn() const { return m_milOn; }

signals:
    void milStatusChanged(bool on);
    void dtcUpdated();

private:
    explicit Dem(QObject *parent = nullptr);
    ~Dem() override = default;

    void loadFromNvm();
    void saveToNvm();
    void updateMil();

    QVector<DtcEntry> m_dtcList;
    bool m_milOn = false;
};

#endif // DEM_H
