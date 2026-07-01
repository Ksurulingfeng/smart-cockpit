/**
 * @file    nvm.h
 * @brief   NvM — 非易失存储薄封装，DEM 通过它持久化 DTC
 *
 * 底层复用 ConfigManager (QSettings INI)，key 前缀 "nvm/"。
 */

#ifndef NVM_H
#define NVM_H

#include <QByteArray>
#include <QString>

namespace Nvm
{
    QByteArray readBlock(const QString &blockId);
    void writeBlock(const QString &blockId, const QByteArray &data);
}

#endif // NVM_H
