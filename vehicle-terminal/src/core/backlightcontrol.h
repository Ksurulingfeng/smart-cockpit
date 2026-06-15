#ifndef BACKLIGHTCONTROL_H
#define BACKLIGHTCONTROL_H

#include <QObject>
#include <QFile>

class BacklightControl : public QObject
{
    Q_OBJECT
public:
    explicit BacklightControl(const QString &sysfsPath = "/sys/class/backlight/backlight",
                              QObject *parent          = nullptr);
    ~BacklightControl() override;

    bool isValid() const
    {
        return m_valid;
    }

    int maxBrightness() const;
    int currentBrightness() const;
    bool setBrightness(int value);          // value 范围 0 ~ maxBrightness()
    bool setBrightnessPercent(int percent); // 0-100

signals:
    void errorOccurred(const QString &msg);

private:
    QString findBrightnessPath(const QString &basePath);
    QFile m_maxFile;
    QFile m_currFile;
    QFile m_brightnessFile;
    bool m_valid;
};

#endif // BACKLIGHTCONTROL_H