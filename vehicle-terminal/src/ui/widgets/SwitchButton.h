#ifndef SWITCHBUTTON_H
#define SWITCHBUTTON_H

#include <QWidget>
#include <QPropertyAnimation>

class SwitchButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int offset READ offset WRITE setOffset)
public:
    explicit SwitchButton(QWidget *parent = nullptr);

    bool isChecked() const
    {
        return m_checked;
    }
    void setChecked(bool checked);

    void setDisabled(bool disabled);
    bool isDisabled() const
    {
        return m_disabled;
    }

    QSize sizeHint() const override;

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    int offset() const
    {
        return m_offset;
    }
    void setOffset(int value);

private:
    void updateAnimation();

    bool m_checked                  = false;
    bool m_disabled                 = false;
    int m_offset                    = 0; // 滑块偏移量（像素）
    int m_sliderRadius              = 0; // 滑块半径
    QPropertyAnimation *m_animation = nullptr;
};

#endif // SWITCHBUTTON_H