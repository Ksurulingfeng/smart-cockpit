#ifndef HOME_PAGE_H
#define HOME_PAGE_H

#include <QWidget>
#include "musicPage.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class HomePage;
}
QT_END_NAMESPACE

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    ~HomePage() override;

    // 公共接口（供外部调用）
    void setMusicController(MusicPage *musicPage); // 设置音乐控制器
    void updateSpeed(int speed);                   // 更新车速 (km/h)
    void updateRpm(int rpm);                       // 更新转速 (rpm)
    void updateOil(int oilX10);                    // 更新油量 (%)
    void updateTemperature(int tempX10);           // 更新车内温度 (×10, 255=25.5°C)
    void updateHumidity(int humX10);               // 更新车内湿度 (×10, 655=65.5%)
    void showMessage(const QString &msg);          // 显示消息 (状态栏或弹窗)

public slots:
    void onWeatherUpdate(const QString &city, int temp, const QString &text);
    void onForecastUpdate(const QStringList &dates,
                          const QList<int> &highs,
                          const QList<int> &lows,
                          const QStringList &texts);
    void onNetworkError(const QString &msg); // 网络错误

private slots:

private:
    void setupIcons();

    Ui::HomePage *ui;
    int m_speed       = 0;  // 车速 km/h
    int m_rpm         = 0;  // 转速 rpm
    int m_oil         = 82; // 油量 %
    int m_temperature = 28; // 车内温度 ℃
    int m_humidity    = 50; // 车内湿度 %
};

#endif // HOME_PAGE_H
