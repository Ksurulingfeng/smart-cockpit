#include "serialAssistantPage.h"
#include "ui_serialAssistantPage.h"
#include "iconhelper.h"
#include <QDateTime>
#include <QMessageBox>

SerialAssistantPage::SerialAssistantPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SerialAssistantPage)
    , m_serial(nullptr)
    , m_hexReceive(false)
    , m_hexSend(false)
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    IconHelper::setupButton(ui->toolButton_back, Qt::black);

    // 创建串口对象
    m_serial = new QSerialPort(this);

    // 连接信号
    connect(ui->toolButton_back, &QToolButton::clicked, this, &SerialAssistantPage::backButtonClicked);
    connect(ui->pushButton_open, &QPushButton::clicked, this, &SerialAssistantPage::onOpenPort);
    connect(ui->pushButton_close, &QPushButton::clicked, this, &SerialAssistantPage::onClosePort);
    connect(ui->pushButton_send, &QPushButton::clicked, this, &SerialAssistantPage::onSendData);
    connect(ui->pushButton_clearReceive, &QPushButton::clicked, this, &SerialAssistantPage::onClearReceive);
    connect(ui->pushButton_clearSend, &QPushButton::clicked, this, &SerialAssistantPage::onClearSend);
    connect(m_serial, &QSerialPort::readyRead, this, &SerialAssistantPage::onReadSerialData);

    // 更新端口列表
    updatePortList();

    // 初始状态：关闭按钮禁用
    ui->pushButton_close->setEnabled(false);
}

SerialAssistantPage::~SerialAssistantPage()
{
    delete ui;
}

void SerialAssistantPage::updatePortList()
{
    ui->comboBox_port->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports) {
        ui->comboBox_port->addItem(port.portName());
    }
    if (ui->comboBox_port->count() == 0) {
        ui->comboBox_port->addItem("无可用串口");
    }
}

void SerialAssistantPage::onOpenPort()
{
    if (ui->comboBox_port->currentText() == "无可用串口") {
        QMessageBox::warning(this, "提示", "没有可用的串口设备！");
        return;
    }

    // 设置串口名
    m_serial->setPortName(ui->comboBox_port->currentText());

    // 设置波特率
    m_serial->setBaudRate(ui->comboBox_baudRate->currentText().toInt());

    // 设置数据位
    switch (ui->comboBox_dataBits->currentText().toInt()) {
        case 5:
            m_serial->setDataBits(QSerialPort::Data5);
            break;
        case 6:
            m_serial->setDataBits(QSerialPort::Data6);
            break;
        case 7:
            m_serial->setDataBits(QSerialPort::Data7);
            break;
        case 8:
            m_serial->setDataBits(QSerialPort::Data8);
            break;
    }

    // 设置校验位
    QString parity = ui->comboBox_parity->currentText();
    if (parity == "None")
        m_serial->setParity(QSerialPort::NoParity);
    else if (parity == "Even")
        m_serial->setParity(QSerialPort::EvenParity);
    else if (parity == "Odd")
        m_serial->setParity(QSerialPort::OddParity);

    // 设置停止位
    QString stopBit = ui->comboBox_stopBits->currentText();
    if (stopBit == "1")
        m_serial->setStopBits(QSerialPort::OneStop);
    else if (stopBit == "1.5")
        m_serial->setStopBits(QSerialPort::OneAndHalfStop);
    else
        m_serial->setStopBits(QSerialPort::TwoStop);

    // 设置流控制（关闭）
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // 打开串口
    if (!m_serial->open(QIODevice::ReadWrite)) {
        QMessageBox::about(NULL, "错误", "串口无法打开！可能串口已经被占用！");
        return;
    }

    // 打开成功，更新 UI
    ui->pushButton_open->setEnabled(false);
    ui->pushButton_close->setEnabled(true);
    ui->comboBox_port->setEnabled(false);
    ui->comboBox_baudRate->setEnabled(false);
    ui->comboBox_dataBits->setEnabled(false);
    ui->comboBox_stopBits->setEnabled(false);
    ui->comboBox_parity->setEnabled(false);
    ui->pushButton_send->setEnabled(true);

    appendReceiveData("串口已打开: " + ui->comboBox_port->currentText());
}

void SerialAssistantPage::onClosePort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        ui->pushButton_open->setEnabled(true);
        ui->pushButton_close->setEnabled(false);
        ui->comboBox_port->setEnabled(true);
        ui->comboBox_baudRate->setEnabled(true);
        ui->comboBox_dataBits->setEnabled(true);
        ui->comboBox_stopBits->setEnabled(true);
        ui->comboBox_parity->setEnabled(true);
        appendReceiveData("串口已关闭");
    }
}

void SerialAssistantPage::onSendData()
{
    if (!m_serial->isOpen()) {
        QMessageBox::warning(this, "提示", "串口未打开！");
        return;
    }

    QString sendData = ui->textEdit_send->toPlainText();
    if (sendData.isEmpty()) return;

    m_hexSend = ui->checkBox_hexSend->isChecked();
    QByteArray data;

    if (m_hexSend) {
        data = hexToBytes(sendData);
    } else {
        data = sendData.toUtf8();
        // 是否添加换行
        if (ui->checkBox_sendNewLine->isChecked()) {
            data.append("\r\n");
        }
    }

    qint64 bytes = m_serial->write(data);
    if (bytes > 0) {
        QString time    = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString display = m_hexSend ? sendData : QString::fromUtf8(data);
        // 是否显示时间戳
        if (ui->checkBox_timestamp->isChecked()) {
            appendReceiveData(QString("[%1] 发送: %2").arg(time).arg(display));
        } else {
            appendReceiveData(display);
        }
    }
}

void SerialAssistantPage::onClearReceive()
{
    ui->textEdit_receive->clear();
}

void SerialAssistantPage::onClearSend()
{
    ui->textEdit_send->clear();
}

void SerialAssistantPage::onReadSerialData()
{
    QByteArray data = m_serial->readAll();
    if (data.isEmpty()) return;

    m_hexReceive = ui->checkBox_hexReceive->isChecked();
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString displayData;

    if (m_hexReceive) {
        displayData = bytesToHex(data);
    } else {
        displayData = QString::fromUtf8(data);
    }

    // 是否显示时间戳
    if (ui->checkBox_timestamp->isChecked()) {
        appendReceiveData(QString("[%1] 接收: %2").arg(time).arg(displayData));
    } else {
        appendReceiveData(displayData);
    }
}

QString SerialAssistantPage::bytesToHex(const QByteArray &bytes)
{
    QString hex;
    for (int i = 0; i < bytes.size(); ++i) {
        hex.append(QString("%1 ").arg(static_cast<quint8>(bytes.at(i)), 2, 16, QChar('0')).toUpper());
    }
    return hex.trimmed();
}

QByteArray SerialAssistantPage::hexToBytes(const QString &hex)
{
    QByteArray bytes;
    QStringList list = hex.split(" ", Qt::SkipEmptyParts);
    for (const QString &s : list) {
        bool ok;
        int val = s.toInt(&ok, 16);
        if (ok && val >= 0 && val <= 255) {
            bytes.append(static_cast<char>(val));
        }
    }
    return bytes;
}

void SerialAssistantPage::appendReceiveData(const QString &data)
{
    ui->textEdit_receive->append(data);
}