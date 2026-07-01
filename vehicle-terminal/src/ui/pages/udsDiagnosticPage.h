/**
 * @file    udsDiagnosticPage.h
 * @brief   UDS 诊断页 — 发送诊断请求、查看响应/DTC、监控节点状态
 */

#ifndef UDSDIAGNOSTICPAGE_H
#define UDSDIAGNOSTICPAGE_H

#include <QWidget>
#include <QVector>
#include <cstdint>

QT_BEGIN_NAMESPACE
namespace Ui { class UdsDiagnosticPage; }
QT_END_NAMESPACE

struct DtcEntry;

class UdsDiagnosticPage : public QWidget
{
    Q_OBJECT
public:
    explicit UdsDiagnosticPage(QWidget *parent = nullptr);
    ~UdsDiagnosticPage() override;

signals:
    void backButtonClicked();

private slots:
    void onSendRequest();
    void onTesterPresent();
    void onClearDtcs();
    void onRefreshDtcs();
    void onServiceChanged(int index);

    void onDiagDataReceived(uint16_t tec, uint16_t rec, uint16_t txCount, uint8_t busLoad);
    void onNodeOnline(int nodeId);
    void onNodeOffline(int nodeId);
    void onMilChanged(bool on);
    void onDcmSessionChanged(uint8_t session);

private:
    void buildUI();
    void setupConnections();
    void appendLog(const QString &text, const QString &color = "");

    QByteArray buildRequest() const;
    void sendUdsRequest(const QByteArray &req);
    void handleUdsResponse(const QByteArray &data);
    QString parseUdsResponse(const QByteArray &data) const;
    QString formatHex(const QByteArray &data) const;

    Ui::UdsDiagnosticPage *ui;
    QVector<DtcEntry> m_dtcCache;
};

#endif // UDSDIAGNOSTICPAGE_H
