#include "MySlider.h"
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

MySlider::MySlider(QWidget *parent)
    : QSlider(parent)
{
}

void MySlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        if (handleRect.contains(event->pos())) {
            QSlider::mousePressEvent(event);
            return;
        }
        double pos;
        if (orientation() == Qt::Horizontal) {
            pos = (double)event->pos().x() / width();
        } else {
            pos = (double)(height() - event->pos().y()) / height();
        }
        int newValue = minimum() + (maximum() - minimum()) * pos;
        setValue(newValue);
        emit sliderMoved(newValue);
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}