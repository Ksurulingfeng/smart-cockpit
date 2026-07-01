/**
 * @file    dem.cpp
 * @brief   DEM 实现 — DTC 存储/上报/清除
 */

#include "dem.h"
#include "nvm.h"
#include <QDebug>

// NVM 持久化 key
#define NVM_KEY_DTC "dem.dtc"

Dem *Dem::instance()
{
    static Dem inst;
    return &inst;
}

Dem::Dem(QObject *parent)
    : QObject(parent)
{
    loadFromNvm();
}

// ==================== 事件上报 ====================

void Dem::reportEvent(uint32_t dtcCode, uint8_t status)
{
    // 查找已存在的 DTC
    for (auto &entry : m_dtcList) {
        if (entry.dtc == dtcCode) {
            entry.statusMask |= status;
            if (status & DTC_STATUS_TEST_FAILED)
                entry.occurrenceCount++;
            goto done;
        }
    }

    // 新的 DTC
    {
        DtcEntry entry;
        entry.dtc             = dtcCode;
        entry.statusMask      = status;
        entry.occurrenceCount = 1;
        m_dtcList.append(entry);
    }

done:
    // 清除标志：testFailed bit 清除意味着故障恢复
    if (!(status & DTC_STATUS_TEST_FAILED)) {
        for (auto &entry : m_dtcList) {
            if (entry.dtc == dtcCode)
                entry.statusMask &= ~DTC_STATUS_TEST_FAILED;
        }
    }

    updateMil();
    saveToNvm();
    emit dtcUpdated();
}

// ==================== DTC 管理 ====================

void Dem::clearAllDtcs()
{
    m_dtcList.clear();
    updateMil();
    saveToNvm();
    emit dtcUpdated();
}

QVector<DtcEntry> Dem::dtcList() const
{
    return m_dtcList;
}

int Dem::dtcCount() const
{
    return m_dtcList.size();
}

// ==================== MIL 管理 ====================

void Dem::updateMil()
{
    bool hasConfirmed = false;
    for (const auto &entry : m_dtcList) {
        if (entry.statusMask & DTC_STATUS_CONFIRMED_DTC) {
            hasConfirmed = true;
            break;
        }
    }
    if (hasConfirmed != m_milOn) {
        m_milOn = hasConfirmed;
        emit milStatusChanged(m_milOn);
    }
}

// ==================== NVM 持久化 ====================

void Dem::loadFromNvm()
{
    QByteArray data = Nvm::readBlock(NVM_KEY_DTC);
    if (data.size() < 8) return; // 最小: 1 个 DTC = 3(DTC)+1(status)+4(occurrence) = 8 字节

    m_dtcList.clear();
    int pos = 0;
    while (pos + 8 <= data.size()) {
        DtcEntry entry;
        uint8_t *p      = (uint8_t *)data.constData() + pos;
        entry.dtc       = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        entry.statusMask = p[3];
        entry.occurrenceCount = ((uint32_t)p[4]) | ((uint32_t)p[5] << 8)
                               | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
        m_dtcList.append(entry);
        pos += 8;
    }
    qDebug() << "[DEM] loaded" << m_dtcList.size() << "DTC(s) from NVM";
    updateMil();
}

void Dem::saveToNvm()
{
    QByteArray data;
    for (const auto &entry : m_dtcList) {
        data.append((char)((entry.dtc >> 16) & 0xFF));
        data.append((char)((entry.dtc >> 8) & 0xFF));
        data.append((char)(entry.dtc & 0xFF));
        data.append((char)entry.statusMask);
        data.append((char)(entry.occurrenceCount & 0xFF));
        data.append((char)((entry.occurrenceCount >> 8) & 0xFF));
        data.append((char)((entry.occurrenceCount >> 16) & 0xFF));
        data.append((char)((entry.occurrenceCount >> 24) & 0xFF));
    }
    Nvm::writeBlock(NVM_KEY_DTC, data);
}
