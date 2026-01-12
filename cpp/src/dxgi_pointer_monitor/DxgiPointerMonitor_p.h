#ifndef TPOINTERMONITOR_P_H
#define TPOINTERMONITOR_P_H

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "DxgiPointerMonitor.h"

#include <QList>
#include <QPoint>

using Microsoft::WRL::ComPtr;

class PointerInfo
{
public:
    PointerInfo();
    ~PointerInfo();

    QByteArray shapeBuffer;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo = {};
    QPoint position;
    bool visible = false;
    int whoUpdatedPositionLast = -1;
    qint64 lastTimeStamp = 0;

    quint64 hash = 0;
    bool changed = false;

    // Getter methods for shape info fields
    // The width in pixels of the mouse cursor.
    UINT width() const { return shapeInfo.Width; }
    // The height in scan lines of the mouse cursor.
    UINT height() const { return shapeInfo.Height; }
    UINT type() const { return shapeInfo.Type; }
    // The position of the cursor's hot spot relative to its upper-left pixel. An application does not use the hot spot when it determines where to draw the cursor shape.
    UINT hotSpotX() const { return shapeInfo.HotSpot.x; }
    UINT hotSpotY() const { return shapeInfo.HotSpot.y; }
    // The width in bytes of the mouse cursor.
    UINT pitch() const { return shapeInfo.Pitch; }

    bool SavePointerToPNG(const QString &filename);
    bool ConvertPointerShapeToQImage(QImage &image);
};

class FrameInfo;

class DisplayDuplication
{
public:
    explicit DisplayDuplication(DxgiPointerMonitorPrivate *monitor);
    ~DisplayDuplication();
public:
    DxgiPointerMonitorPrivate *monitor = nullptr;

    ComPtr<ID3D11Device> d3d11Device;
    ComPtr<IDXGIOutputDuplication> deskDupl;
    DXGI_OUTPUT_DESC outputDesc = {};
    int displayIndex = -1;

public:
    enum class FrameReturn {
        Success,
        Timeout,
        Failure
    };
    bool getPointerInfo(DXGI_OUTDUPL_FRAME_INFO *frameInfo) const;
    FrameReturn getFrame(FrameInfo &frameInfoHolder) const;
};

class DxgiPointerMonitorPrivate
{
    Q_DECLARE_PUBLIC(DxgiPointerMonitor)

public:
    DxgiPointerMonitorPrivate();
    virtual ~DxgiPointerMonitorPrivate();
protected:
    DxgiPointerMonitor *q_ptr = nullptr;

public:
    bool initialized = false;
protected:
    void resetDisplayDuplications();
public:
    PointerInfo pointerInfo;
    QList<DisplayDuplication *> displayDuplications;
    qint64 lastHash = 0;
    int imageCounter = 0;
    bool isFirst = true;
};

class FrameInfo
{
public:
    explicit FrameInfo(DisplayDuplication *duplication, DXGI_OUTDUPL_FRAME_INFO *frameInfo);
    ~FrameInfo();

public:
    DXGI_OUTDUPL_FRAME_INFO *inner = nullptr;
    DisplayDuplication *owner = nullptr;
    bool valid = false;
};

#endif // TPOINTERMONITOR_P_H
