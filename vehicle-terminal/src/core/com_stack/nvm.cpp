/**
 * @file    nvm.cpp
 * @brief   NvM 实现
 */

#include "nvm.h"
#include "configmanager.h"

namespace Nvm
{

QByteArray readBlock(const QString &blockId)
{
    QString key = "nvm/" + blockId;
    QString hex = ConfigManager::instance()->value(key, "").toString();
    if (hex.isEmpty())
        return QByteArray();
    return QByteArray::fromHex(hex.toUtf8());
}

void writeBlock(const QString &blockId, const QByteArray &data)
{
    QString key = "nvm/" + blockId;
    ConfigManager::instance()->setValue(key, QString::fromUtf8(data.toHex()));
}

} // namespace Nvm
