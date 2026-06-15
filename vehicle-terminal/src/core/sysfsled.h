#ifndef SYSFSLED_H
#define SYSFSLED_H

#include <QObject>
#include <QFile>
#include <QString>

class SysfsLed : public QObject
{
    Q_OBJECT
public:
    // path: 例如 "/sys/class/leds/sys-led/brightness"
    explicit SysfsLed(const QString &path, QObject *parent = nullptr);
    ~SysfsLed() override;

    bool isValid() const
    {
        return m_valid;
    }

    void setState(bool on);
    bool state() const;

    void setTrigger(const QString &trigger); // 例如 "heartbeat", "none"
    QString trigger() const;

signals:
    void errorOccurred(const QString &msg);

private:
    QFile m_brightnessFile;
    QFile m_triggerFile;
    bool m_valid;
};

#endif // SYSFSLED_H