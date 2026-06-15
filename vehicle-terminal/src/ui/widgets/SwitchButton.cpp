#include "SwitchButton.h"
#include <QPainter>
#include <QMouseEvent>

SwitchButton::SwitchButton(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(50, 30); // 默认大小，可通过resize或布局调整
    setCursor(Qt::PointingHandCursor);

    m_animation = new QPropertyAnimation(this, "offset");
    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::OutQuad);
}

void SwitchButton::setChecked(bool checked)
{
    if (m_checked == checked)
        return;
    m_checked = checked;
    updateAnimation();
    update();
    emit toggled(m_checked);
}

void SwitchButton::setDisabled(bool disabled)
{
    if (m_disabled == disabled)
        return;
    m_disabled = disabled;
    setAttribute(Qt::WA_TransparentForMouseEvents, disabled);
    update();
}

QSize SwitchButton::sizeHint() const
{
    return QSize(50, 30);
}

void SwitchButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景矩形尺寸（留出滑块空间）
    int bgHeight = height() - 4;
    int bgWidth  = width() - 4;
    int bgX      = 2;
    int bgY      = 2;

    // 滑块半径
    m_sliderRadius = bgHeight / 2;
    // 滑块偏移量范围：0 到 (bgWidth - 2*m_sliderRadius)
    int maxOffset = bgWidth - 2 * m_sliderRadius;
    int sliderX   = bgX + m_offset;
    int sliderY   = bgY + (bgHeight / 2 - m_sliderRadius);

    // 绘制背景（圆角矩形）
    QRectF bgRect(bgX, bgY, bgWidth, bgHeight);
    QColor bgColor;
    if (m_disabled) {
        bgColor = QColor(200, 200, 200);
    } else {
        bgColor = m_checked ? QColor(52, 168, 83) : QColor(190, 190, 190);
    }
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(bgRect, bgHeight / 2, bgHeight / 2);

    // 绘制滑块（圆形）
    QColor sliderColor = m_disabled ? QColor(150, 150, 150) : QColor(255, 255, 255);
    painter.setBrush(sliderColor);
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawEllipse(sliderX, sliderY, m_sliderRadius * 2, m_sliderRadius * 2);
}

void SwitchButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_disabled) {
        setChecked(!m_checked);
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void SwitchButton::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    update();
}

void SwitchButton::setOffset(int value)
{
    m_offset = value;
    update();
}

void SwitchButton::updateAnimation()
{
    int bgHeight     = height() - 4;
    int bgWidth      = width() - 4;
    m_sliderRadius   = bgHeight / 2;
    int maxOffset    = bgWidth - 2 * m_sliderRadius;
    int targetOffset = m_checked ? maxOffset : 0;

    m_animation->stop();
    m_animation->setEndValue(targetOffset);
    m_animation->start();
}