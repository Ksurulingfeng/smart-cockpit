#ifndef ICONHELPER_H
#define ICONHELPER_H

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QAbstractButton>

class IconHelper
{
public:
    // 给图标染色（保持原始大小）
    static QIcon tintIcon(const QIcon &icon, const QColor &color)
    {
        // 获取原始图标的大小
        QList<QSize> sizes = icon.availableSizes();
        if (sizes.isEmpty()) {
            // 如果没有可用大小，使用默认大小
            QPixmap pixmap = icon.pixmap(QSize(32, 32));
            QPainter painter(&pixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pixmap.rect(), color);
            painter.end();
            return QIcon(pixmap);
        }

        // 使用原始大小
        QSize originalSize = sizes.first();
        QPixmap pixmap     = icon.pixmap(originalSize);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }

    static void setColor(QAbstractButton *btn, const QColor &color)
    {
        QIcon newIcon = tintIcon(btn->icon(), color);
        btn->setIcon(newIcon);
    }

    // 为按钮设置双色图标（正常/按下），保持原始图标大小
    static void setupButton(QAbstractButton *btn, const QColor &normalColor, const QColor &pressedColor)
    {
        if (!btn) return;

        QIcon baseIcon    = btn->icon();
        QIcon normalIcon  = tintIcon(baseIcon, normalColor);
        QIcon pressedIcon = tintIcon(baseIcon, pressedColor); // 直接从原始图标染色，避免叠加

        btn->setIcon(normalIcon);
        // 不设置图标大小，保持原来的

        // 断开旧连接避免重复
        btn->disconnect(btn, &QAbstractButton::pressed, nullptr, nullptr);
        btn->disconnect(btn, &QAbstractButton::released, nullptr, nullptr);

        // 建立新连接
        QObject::connect(btn, &QAbstractButton::pressed, btn, [btn, pressedIcon]() {
            btn->setIcon(pressedIcon);
        });
        QObject::connect(btn, &QAbstractButton::released, btn, [btn, normalIcon]() {
            btn->setIcon(normalIcon);
        });
    }

    // 简化版：按下颜色自动为灰色
    static void setupButton(QAbstractButton *btn, const QColor &normalColor)
    {
        setupButton(btn, normalColor, QColor(128, 128, 128));
    }
};

#endif // ICONHELPER_H