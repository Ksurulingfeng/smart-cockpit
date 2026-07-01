/**
 * @file    udsDiagnosticPage.cpp
 * @brief   UDS 诊断页实现
 */

#include "udsDiagnosticPage.h"
#include "ui_udsDiagnosticPage.h"
#include "com_stack/com.h"
#include "com_stack/dcm.h"
#include "com_stack/dem.h"
#include "rte.h"
#include "iconhelper.h"
#include <QDateTime>
#include <QDebug>
#include <QScrollBar>

// UDS 服务列表
static const struct { uint8_t sid; const char *name; } UDS_SERVICES[] = {
    { 0x10, "0x10 诊断会话控制" },
    { 0x22, "0x22 按ID读数据"    },
    { 0x19, "0x19 读DTC信息"     },
    { 0x14, "0x14 清除DTC"       },
    { 0x11, "0x11 ECU复位"       },
    { 0x3E, "0x3E 诊断会话保持"   },
};

static const struct { uint16_t did; const char *name; } DID_LIST[] = {
    { 0xF100, "0xF100 ECU序列号"      },
    { 0xF190, "0xF190 VIN码"           },
    { 0xF1A0, "0xF1A0 软件版本"        },
    { 0xF1B0, "0xF1B0 总线负载"        },
    { 0xF1B1, "0xF1B1 NodeA TEC"       },
    { 0xF1B2, "0xF1B2 NodeA REC"       },
    { 0xF1B3, "0xF1B3 NodeA TX计数"    },
    { 0xF1C1, "0xF1C1 NodeB TEC"       },
    { 0xF1C2, "0xF1C2 NodeB REC"       },
    { 0xF1C3, "0xF1C3 NodeB TX计数"    },
};

static const char *sessionName(uint8_t s) {
    switch (s) {
    case 0x01: return "默认";
    case 0x02: return "编程";
    case 0x03: return "扩展";
    default: return "?";
    }
}

// ==================== 构造 / 析构 ====================

UdsDiagnosticPage::UdsDiagnosticPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UdsDiagnosticPage)
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    IconHelper::setupButton(ui->toolButton_back, Qt::black);

    buildUI();
    setupConnections();

    // 初始化节点在线状态（页面打开时节点可能已在线，不会再发 nodeOnline 信号）
    for (int n = 0; n < 2; ++n) {
        if (Rte::instance()->isNodeOnline(n))
            onNodeOnline(n);
        else
            onNodeOffline(n);
    }

    onRefreshDtcs();
    appendLog("UDS 诊断页就绪", "green");
}

UdsDiagnosticPage::~UdsDiagnosticPage()
{
    delete ui;
}

// ==================== UI 初始化 ====================

void UdsDiagnosticPage::buildUI()
{
    for (auto &svc : UDS_SERVICES)
        ui->comboBox_service->addItem(svc.name, svc.sid);
    ui->comboBox_service->setCurrentIndex(1); // 默认 0x22

    for (auto &d : DID_LIST)
        ui->comboBox_did->addItem(d.name, d.did);

    // 目标节点选择（加到请求区布局的行尾）
    QComboBox *nodeCombo = new QComboBox(this);
    nodeCombo->addItem("NodeA (0x7E0/0x7E8)", 0);
    nodeCombo->addItem("NodeB (0x7E1/0x7E9)", 1);
    connect(nodeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int idx) {
        Dcm::instance()->setTargetNode(idx);
    });
    ui->gridLayout_request->addWidget(new QLabel("目标节点:", this), 3, 0);
    ui->gridLayout_request->addWidget(nodeCombo, 3, 1);
    Dcm::instance()->setTargetNode(0);
}

// ==================== 信号连接 ====================

void UdsDiagnosticPage::setupConnections()
{
    connect(ui->toolButton_back, &QToolButton::clicked,
            this, &UdsDiagnosticPage::backButtonClicked);
    connect(ui->pushButton_send, &QPushButton::clicked,
            this, &UdsDiagnosticPage::onSendRequest);
    connect(ui->pushButton_testerPresent, &QPushButton::clicked,
            this, &UdsDiagnosticPage::onTesterPresent);
    connect(ui->pushButton_clearDtc, &QPushButton::clicked,
            this, &UdsDiagnosticPage::onClearDtcs);
    connect(ui->pushButton_refreshDtc, &QPushButton::clicked,
            this, &UdsDiagnosticPage::onRefreshDtcs);
    connect(ui->pushButton_clearLog, &QPushButton::clicked, this, [this]() {
        ui->textEdit_response->clear();
        appendLog("日志已清空", "");
    });
    connect(ui->comboBox_service, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UdsDiagnosticPage::onServiceChanged);

    // 诊断数据
    connect(Com::instance(), &Com::diagnosticDataReceived,
            this, &UdsDiagnosticPage::onDiagDataReceived);

    // 节点上下线
    connect(Rte::instance(), &Rte::nodeOnline,
            this, &UdsDiagnosticPage::onNodeOnline);
    connect(Rte::instance(), &Rte::nodeOffline,
            this, &UdsDiagnosticPage::onNodeOffline);

    // MIL / 会话
    connect(Dem::instance(), &Dem::milStatusChanged,
            this, &UdsDiagnosticPage::onMilChanged);
    connect(Dcm::instance(), &Dcm::sessionChanged,
            this, &UdsDiagnosticPage::onDcmSessionChanged);

    // UDS 响应
    connect(Dcm::instance(), &Dcm::sendUdsResponse,
            this, &UdsDiagnosticPage::handleUdsResponse);
}

// ==================== UDS 请求 ====================

QByteArray UdsDiagnosticPage::buildRequest() const
{
    QByteArray req;
    uint8_t sid = (uint8_t)ui->comboBox_service->currentData().toUInt();
    req.append((char)sid);

    switch (sid) {
    case 0x10:
        req.append((char)0x02); // 请求编程会话
        break;
    case 0x22: {
        uint16_t did = (uint16_t)ui->comboBox_did->currentData().toUInt();
        QString custom = ui->lineEdit_did->text().trimmed();
        if (!custom.isEmpty()) {
            bool ok;
            uint16_t v = custom.toUShort(&ok, 16);
            if (ok) did = v;
        }
        req.append((char)((did >> 8) & 0xFF));
        req.append((char)(did & 0xFF));
        break;
    }
    case 0x19:
        req.append((char)0x02); // reportDTCByStatusMask
        req.append((char)0xFF);
        break;
    case 0x14:
        break;
    case 0x11:
        req.append((char)0x01); // 硬复位
        break;
    case 0x3E:
        req.append((char)0x00);
        break;
    }
    return req;
}

void UdsDiagnosticPage::sendUdsRequest(const QByteArray &req)
{
    appendLog("→ " + formatHex(req), "#4a9eff");
    Dcm::instance()->sendUdsRequest(req);
}

// ==================== UDS 响应 ====================

void UdsDiagnosticPage::handleUdsResponse(const QByteArray &data)
{
    QString text = parseUdsResponse(data);
    if (data.size() > 0 && (uint8_t)data[0] == 0x7F)
        appendLog("← " + text, "orange");
    else
        appendLog("← " + text, "#4caf50");
}

QString UdsDiagnosticPage::parseUdsResponse(const QByteArray &data) const
{
    if (data.isEmpty()) return "(empty)";

    QString hex  = formatHex(data);
    uint8_t first = (uint8_t)data[0];

    if (first == 0x7F && data.size() >= 3) {
        uint8_t reqSid = data[1], nrc = data[2];
        QString nrcDesc;
        switch (nrc) {
        case 0x11: nrcDesc = "服务不支持"; break;
        case 0x12: nrcDesc = "子功能不支持"; break;
        case 0x22: nrcDesc = "条件不满足"; break;
        case 0x31: nrcDesc = "请求超出范围"; break;
        default: nrcDesc = "未知"; break;
        }
        return QString("负响应 SID=0x%1 NRC=0x%2 (%3) | %4")
            .arg(reqSid, 2, 16, QChar('0'))
            .arg(nrc, 2, 16, QChar('0')).arg(nrcDesc).arg(hex);
    }

    uint8_t reqSid = first - 0x40;

    switch (reqSid) {
    case 0x10: {
        uint8_t s = (data.size() >= 2) ? (uint8_t)data[1] : 0;
        return QString("正响应 会话→%1 (%2) | %3").arg(s).arg(sessionName(s)).arg(hex);
    }
    case 0x22: {
        uint16_t did = (data.size() >= 3)
            ? ((uint16_t)(uint8_t)data[1] << 8) | (uint8_t)data[2] : 0;
        QByteArray val = data.mid(3);

        // 数值型 DID → 解析为整数
        if (did == 0xF1B0 || did == 0xF1B1 || did == 0xF1B2 || did == 0xF1B3
            || did == 0xF1C1 || did == 0xF1C2 || did == 0xF1C3) {
            uint16_t num = 0;
            if (val.size() >= 2)
                num = (uint8_t)val[0] | ((uint8_t)val[1] << 8);
            else if (val.size() >= 1)
                num = (uint8_t)val[0];
            return QString("正响应 DID=0x%1 → %2 | %3")
                .arg(did, 4, 16, QChar('0')).arg(num).arg(hex);
        }

        return QString("正响应 DID=0x%1 → \"%2\" | %3")
            .arg(did, 4, 16, QChar('0')).arg(QString::fromUtf8(val)).arg(hex);
    }
    case 0x19: {
        int cnt = (data.size() >= 3) ? (data.size() - 2) / 4 : 0;
        QStringList lines;
        lines.append(QString("正响应 读DTC → %1 条 | %2").arg(cnt).arg(hex));
        for (int i = 0; i < cnt && (3 + i * 4 + 3) < data.size(); ++i) {
            int off = 2 + i * 4;
            uint32_t dtc = ((uint32_t)(uint8_t)data[off] << 16)
                         | ((uint32_t)(uint8_t)data[off+1] << 8)
                         | (uint8_t)data[off+2];
            uint8_t st = data[off+3];
            lines.append(QString("  DTC 0x%1 status=0x%2")
                .arg(dtc, 6, 16, QChar('0')).arg(st, 2, 16, QChar('0')));
        }
        return lines.join('\n');
    }
    case 0x14: return QString("正响应 DTC已清除 | %1").arg(hex);
    case 0x11: return QString("正响应 ECU复位 | %1").arg(hex);
    case 0x3E: return QString("正响应 保活 | %1").arg(hex);
    default:
        return QString("正响应 SID=0x%1 | %2").arg(reqSid, 2, 16, QChar('0')).arg(hex);
    }
}

// ==================== 按钮槽 ====================

void UdsDiagnosticPage::onSendRequest()
{
    sendUdsRequest(buildRequest());
}

void UdsDiagnosticPage::onTesterPresent()
{
    QByteArray req;
    req.append((char)0x3E);
    req.append((char)0x00);
    sendUdsRequest(req);
}

void UdsDiagnosticPage::onClearDtcs()
{
    QByteArray req;
    req.append((char)0x14);
    sendUdsRequest(req);
    Dem::instance()->clearAllDtcs();
    appendLog("DTC 已清除", "green");
}

void UdsDiagnosticPage::onRefreshDtcs()
{
    m_dtcCache = Dem::instance()->dtcList();
    if (m_dtcCache.isEmpty()) {
        appendLog("DTC: (空)", "");
        return;
    }
    appendLog(QString("DTC: %1 条").arg(m_dtcCache.size()), "");
    for (const auto &e : m_dtcCache) {
        QString st;
        if (e.statusMask & (1 << 3)) st = "已确认";
        else if (e.statusMask & (1 << 2)) st = "待定";
        else if (e.statusMask & (1 << 0)) st = "当前故障";
        else st = QString("0x%1").arg(e.statusMask, 2, 16, QChar('0'));
        appendLog(QString("  0x%1  [%2] ×%3")
            .arg(e.dtc, 6, 16, QChar('0')).arg(st).arg(e.occurrenceCount), "");
    }
}

void UdsDiagnosticPage::onServiceChanged(int index)
{
    uint8_t sid = (uint8_t)ui->comboBox_service->itemData(index).toUInt();
    bool show = (sid == 0x22);
    ui->comboBox_did->setVisible(show);
    ui->lineEdit_did->setVisible(show);
}

// ==================== 状态回调 ====================

void UdsDiagnosticPage::onDiagDataReceived(uint16_t tec, uint16_t rec,
                                            uint16_t txCount, uint8_t busLoad)
{
    ui->label_diagData->setText(
        QString("TEC:%1 REC:%2 TX:%3 负载:%4%")
            .arg(tec).arg(rec).arg(txCount).arg(busLoad));
}

void UdsDiagnosticPage::onNodeOnline(int nodeId)
{
    QLabel *lbl = (nodeId == 0) ? ui->label_nodeA : ui->label_nodeB;
    QChar n = (nodeId == 0) ? 'A' : 'B';
    lbl->setText(QString("Node%1: ●在线").arg(n));
    lbl->setStyleSheet("color:#4caf50;font-weight:bold;");
    appendLog(QString("节点 %1 上线").arg(n), "green");
}

void UdsDiagnosticPage::onNodeOffline(int nodeId)
{
    QLabel *lbl = (nodeId == 0) ? ui->label_nodeA : ui->label_nodeB;
    QChar n = (nodeId == 0) ? 'A' : 'B';
    lbl->setText(QString("Node%1: ○离线").arg(n));
    lbl->setStyleSheet("color:#f44336;font-weight:bold;");
    appendLog(QString("节点 %1 离线").arg(n), "red");
}

void UdsDiagnosticPage::onMilChanged(bool on)
{
    ui->label_mil->setText(on ? "MIL: ●" : "MIL: ○");
    ui->label_mil->setStyleSheet(
        on ? "color:#f44336;font-weight:bold;" : "font-weight:bold;");
}

void UdsDiagnosticPage::onDcmSessionChanged(uint8_t session)
{
    ui->label_session->setText(QString("会话: %1").arg(sessionName(session)));
}

// ==================== 工具函数 ====================

void UdsDiagnosticPage::appendLog(const QString &text, const QString &color)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line;
    if (color.isEmpty())
        line = QString("[%1] %2").arg(time).arg(text);
    else
        line = QString("<span style='color:%1'>[%2] %3</span>")
                   .arg(color).arg(time).arg(text.toHtmlEscaped());

    ui->textEdit_response->append(line);
    QScrollBar *sb = ui->textEdit_response->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

QString UdsDiagnosticPage::formatHex(const QByteArray &data) const
{
    QString r;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) r += ' ';
        r += QString("%1").arg((uint8_t)data[i], 2, 16, QChar('0')).toUpper();
    }
    return r;
}
