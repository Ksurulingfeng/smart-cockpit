#ifndef SERIAL_ASSISTANT_PAGE_H
#define SERIAL_ASSISTANT_PAGE_H

#include <QWidget>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class SerialAssistantPage;
}
QT_END_NAMESPACE

class SerialAssistantPage : public QWidget
{
    Q_OBJECT

public:
    explicit SerialAssistantPage(QWidget *parent = nullptr);
    ~SerialAssistantPage() override;

signals:
    void backButtonClicked();

private slots:
    void onOpenPort();
    void onClosePort();
    void onSendData();
    void onClearReceive();
    void onClearSend();
    void onReadSerialData();

private:
    void updatePortList();
    QString bytesToHex(const QByteArray &bytes);
    QByteArray hexToBytes(const QString &hex);
    void appendReceiveData(const QString &data);

private:
    Ui::SerialAssistantPage *ui;
    QSerialPort *m_serial;
    bool m_hexReceive;
    bool m_hexSend;
};

#endif