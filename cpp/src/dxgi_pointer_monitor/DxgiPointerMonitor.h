#ifndef TPOINTERMONITOR_H
#define TPOINTERMONITOR_H

#include <QObject>

class DxgiPointerMonitorPrivate;
class DxgiPointerMonitor : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(DxgiPointerMonitor)

public:
    explicit DxgiPointerMonitor(QObject *parent = nullptr);
    ~DxgiPointerMonitor() override;
protected:
    QScopedPointer<DxgiPointerMonitorPrivate> d_ptr;

public:
    // 注意 Windows 开启某些优化项之后，指针调整到 10 及以上，指针层会总是隐藏，指针在桌面图像层绘制
    bool capture(bool &visible, QPoint &position, QPoint &hotSpot, QByteArray &cursorData, bool &changed);
};

#endif // TPOINTERMONITOR_H
