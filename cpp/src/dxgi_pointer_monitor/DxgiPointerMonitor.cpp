#include "DxgiPointerMonitor.h"
#include "DxgiPointerMonitor_p.h"

#include <QPoint>
#include <QStringView>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QImage>
#include <QColor>
#include <QPixmap>
#include <QBuffer>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPointerMonitor, "DxgiPointerMonitor")

FrameInfo::FrameInfo(DisplayDuplication* duplication, DXGI_OUTDUPL_FRAME_INFO* frameInfo)
    : owner(duplication)
    , inner(frameInfo)
{
}

FrameInfo::~FrameInfo()
{
    if (valid && owner && inner) {
        HRESULT hr = owner->deskDupl->ReleaseFrame();
        if (FAILED(hr)) {
            qCCritical(lcPointerMonitor, "Failed to release frame in FrameInfo destructor");
        }
        valid = false;
        inner = nullptr;
    }
}

PointerInfo::PointerInfo() {}

PointerInfo::~PointerInfo()
{
    qCDebug(lcPointerMonitor, "Destructed PointerInfo");
}

DisplayDuplication::DisplayDuplication(DxgiPointerMonitorPrivate* monitor)
    : monitor(monitor)
{
    RtlZeroMemory(&outputDesc, sizeof(outputDesc));
}

DisplayDuplication::~DisplayDuplication()
{
    RtlZeroMemory(&outputDesc, sizeof(outputDesc));
    qCDebug(lcPointerMonitor, "Destructed DisplayDuplication");
}

DxgiPointerMonitor::DxgiPointerMonitor(QObject* parent)
    : QObject(parent)
    , d_ptr(new DxgiPointerMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

DxgiPointerMonitor::~DxgiPointerMonitor()
{
}

DxgiPointerMonitorPrivate::DxgiPointerMonitorPrivate()
{
    // Create pointer_bmps directory if it doesn't exist
    QDir dir("pointer_pngs");
    if (!dir.exists()) {
        dir.mkpath(".");
        qCInfo(lcPointerMonitor, "Created pointer_bmps directory");
    }

    resetDisplayDuplications();
    if (displayDuplications.isEmpty()) {
        qCCritical(lcPointerMonitor, "Failed to Initialize DxgiPointerMonitor");
    }
}

DxgiPointerMonitorPrivate::~DxgiPointerMonitorPrivate()
{
    qDeleteAll(displayDuplications);
    displayDuplications.clear();
}

void DxgiPointerMonitorPrivate::resetDisplayDuplications()
{
    HRESULT hr = S_OK;

    qDeleteAll(displayDuplications);
    displayDuplications.clear();

    // Driver types supported
    D3D_DRIVER_TYPE driverTypes[] = {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE};
    UINT numDriverTypes = ARRAYSIZE(driverTypes);
    // Feature levels supported
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_1};
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_1_0_CORE;

    INT currentIndex = 0;

    // 循环获取所有显示器上下文
    while (true) {
        auto displayDuplication = new DisplayDuplication(this);

        // Create device
        for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; ++driverTypeIndex) {
            hr = D3D11CreateDevice(nullptr, driverTypes[driverTypeIndex], nullptr, 0,
                                   featureLevels, numFeatureLevels, D3D11_SDK_VERSION,
                                   &displayDuplication->d3d11Device, &featureLevel,
                                   nullptr);
            if (SUCCEEDED(hr)) {
                // Device creation success, no need to loop anymore
                break;
            }
        }
        if (FAILED(hr)) {
            qCCritical(lcPointerMonitor, "Failed to create device in InitializeDx");
            return;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        hr = displayDuplication->d3d11Device->QueryInterface(dxgiDevice.GetAddressOf());

        if (FAILED(hr)) {
            qCCritical(lcPointerMonitor, "Failed to QI for DXGI Device");
            return;
        }

        ComPtr<IDXGIAdapter> dxgiAdapter;
        hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(dxgiAdapter.GetAddressOf()));

        if (FAILED(hr)) {
            qCCritical(lcPointerMonitor, "Failed to get DXGI Adapter");
            return;
        }

        ComPtr<IDXGIOutput> dxgiOutput;
        hr = dxgiAdapter->EnumOutputs(currentIndex, &dxgiOutput);
        if (!dxgiOutput || FAILED(hr)) {
            qCInfo(lcPointerMonitor, "Finished finding displays");
            break;
        }
        dxgiOutput->GetDesc(&displayDuplication->outputDesc);

        // QI for Output 1
        ComPtr<IDXGIOutput1> dxgiOutput1;
        hr = dxgiOutput->QueryInterface(dxgiOutput1.GetAddressOf());
        if (FAILED(hr)) {
            qCCritical(lcPointerMonitor, "Failed to QI for DxgiOutput1");
            return;
        }

        // Create desktop duplication
        hr = dxgiOutput1->DuplicateOutput(displayDuplication->d3d11Device.Get(), &displayDuplication->deskDupl);
        if (FAILED(hr)) {
            qCCritical(lcPointerMonitor, " Failed to create desktop duplication for output %d", currentIndex);
            return;
        }

        if (FAILED(hr)) {
            if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
                qCCritical(lcPointerMonitor, " Failed to get duplicate output: There is "
                                             "already the maximum number of applications using the Desktop "
                                             "Duplication API running, please close one of those applications "
                                             "and then try again.");
            }
            return;
        }

        displayDuplication->displayIndex = currentIndex++;

        displayDuplications.append(displayDuplication);
    }

    initialized = true;

    qCInfo(lcPointerMonitor, "Display count: %lld", displayDuplications.size());
}

bool DxgiPointerMonitor::capture(bool& visible, QPoint& position, QPoint& hotSpot, QByteArray& cursorData, bool& changed)
{
    Q_D(DxgiPointerMonitor);

    // 由于首次光标为隐藏时，抓不到任何信息，所以默认为隐藏，后续抓到后会立即设置回显示
    if (d->isFirst) {
        visible = false;
        changed = true;
        d->isFirst = false;

        RECT rc;
        if (GetWindowRect(GetDesktopWindow(), &rc)) {
            LONG w = rc.right - rc.left;
            LONG h = rc.bottom - rc.top;
            position = QPoint(w / 2, h / 2);
        }
        qCInfo(lcPointerMonitor, "First poll, set visible to false and put to screen middle");
        return true;
    }

    d->pointerInfo.changed = false;

    // 循环阻塞运行
    if (d->displayDuplications.isEmpty()) {
        d->resetDisplayDuplications();
        if (d->displayDuplications.isEmpty()) {
            qCWarning(lcPointerMonitor, "Failed to reinitialize DxgiPointerMonitor, probably due to resolution or scale adjusting, wait for next poll");
            return false;
        }
    }

    for (DisplayDuplication* displayDuplication : d->displayDuplications) {
        DXGI_OUTDUPL_FRAME_INFO frameInfo{};
        FrameInfo frameInfoHolder(displayDuplication, &frameInfo);

        switch (displayDuplication->getFrame(frameInfoHolder)) {
        case DisplayDuplication::FrameReturn::Success:
            d->isFirst = false;
            break;
        case DisplayDuplication::FrameReturn::Timeout:
            d->isFirst = false;
            continue;
        case DisplayDuplication::FrameReturn::Failure:
            // 尝试重新初始化
            qDeleteAll(d->displayDuplications);
            d->displayDuplications.clear();
            return false;
        }

        if (!displayDuplication->getPointerInfo(frameInfoHolder.inner)) {
            continue;
        }
        if (!d->pointerInfo.changed) {
            continue;
        }

#if 0
        // NOTE: 调试使用
        qCInfo(lcPointerMonitor,
               "Visible: %d, type: %d, Pos: [%d,%d], Size: [%dx%d], Hotspot: "
               "[%d,%d], "
               "Pitch: %d, "
               "ShapeHash: 0x%016llx, "
               "Index: %d",
               d->pointerInfo.visible, d->pointerInfo.type(),
               d->pointerInfo.position.x(), d->pointerInfo.position.y(),
               d->pointerInfo.width(),
               d->pointerInfo.height(),
               d->pointerInfo.hotSpotX(),
               d->pointerInfo.hotSpotY(),
               d->pointerInfo.pitch(), d->pointerInfo.hash,
               displayDuplication->displayIndex);
        // Save pointer image to BMP and PNG files if there's a shape change
        if (!d->pointerInfo.shapeBuffer.isEmpty()) {
            // Increment image counter for unique filename
            d->imageCounter++;

            // Create a unique filename based on timestamp, counter and shape hash
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
            QString base_filename = QString("pointer_pngs/pointer_%1").arg(d->imageCounter);

            QString png_filename = base_filename + ".png";

            // Save the pointer image to PNG file
            bool png_saved = d->pointerInfo.SavePointerToPNG(png_filename);

            if (png_saved) {
                 qCInfo(lcPointerMonitor, "Saved pointer image to: %s", qPrintable(png_filename));
            } else {
                qCWarning(lcPointerMonitor, "Failed to save pointer image to: %s", qPrintable(png_filename));
            }
        }
#endif

        visible = d->pointerInfo.visible;
        position = d->pointerInfo.position;
        hotSpot = QPoint(static_cast<int>(d->pointerInfo.hotSpotX()), static_cast<int>(d->pointerInfo.hotSpotY()));
        changed = d->pointerInfo.changed;

        // Convert the pointer shape data to PNG format
        if (!d->pointerInfo.shapeBuffer.isEmpty()) {
            QImage image;
            if (d->pointerInfo.ConvertPointerShapeToQImage(image)) {
                // Convert QImage to PNG QByteArray
                QBuffer buffer(&cursorData);
                buffer.open(QIODevice::WriteOnly);
                if (!image.save(&buffer, "PNG")) {
                    qCWarning(lcPointerMonitor, "Failed to convert cursor image to PNG format");
                    cursorData = QByteArray(); // Set to empty if save fails
                }
            } else {
                qCWarning(lcPointerMonitor, "Failed to convert pointer shape to QImage");
                cursorData = QByteArray();
            }
        } else {
            cursorData = QByteArray(); // No cursor data
        }
        break;
    }
    return true;
}

bool DisplayDuplication::getPointerInfo(DXGI_OUTDUPL_FRAME_INFO* frameInfo) const
{
    // A non-zero mouse update timestamp indicates that there is a mouse
    // position update and optionally a shape change
    if (frameInfo->LastMouseUpdateTime.QuadPart == 0) {
        return false;
    }

    bool updatePosition = true;

    // Make sure we don't update pointer position wrongly
    // If pointer is invisible, make sure we did not get an update from another
    // output that the last time that said pointer was visible, if so, don't set
    // it to invisible or update.
    if (!frameInfo->PointerPosition.Visible && (monitor->pointerInfo.whoUpdatedPositionLast != displayIndex)) {
        updatePosition = false;
    }

    // If two outputs both say they have a visible, only update if new update has
    // newer timestamp
    if (frameInfo->PointerPosition.Visible && monitor->pointerInfo.visible && monitor->pointerInfo.whoUpdatedPositionLast != displayIndex && monitor->pointerInfo.lastTimeStamp > frameInfo->LastMouseUpdateTime.QuadPart) {
        updatePosition = false;
    }

    POINT cursorPos{0, 0};
    // 尝试使用 windows 抓取光标位置，物理坐标
    if (!GetCursorPos(&cursorPos)) {
        qWarning(lcPointerMonitor, "GetCursorPos failed, LastError: %lu", GetLastError());
    }

    // Update position
    if (updatePosition) {
        if (monitor->pointerInfo.whoUpdatedPositionLast != displayIndex) {
            monitor->pointerInfo.whoUpdatedPositionLast = displayIndex;
            monitor->pointerInfo.changed = true;
        }
        monitor->pointerInfo.lastTimeStamp = frameInfo->LastMouseUpdateTime.QuadPart;
        if (monitor->pointerInfo.visible != (frameInfo->PointerPosition.Visible != 0)) {
            monitor->pointerInfo.visible = frameInfo->PointerPosition.Visible != 0;
            monitor->pointerInfo.changed = true;
        }

        monitor->pointerInfo.position.setX(cursorPos.x);
        monitor->pointerInfo.position.setY(cursorPos.y);
    }

    // No new shape
    if (frameInfo->PointerShapeBufferSize == 0) {
        if (monitor->pointerInfo.visible) {
            return false;
        }
        // 判断光标是否在当前显示器区域内
        bool cursorInDisplay = cursorPos.x >= outputDesc.DesktopCoordinates.left && cursorPos.x < outputDesc.DesktopCoordinates.right
                               && cursorPos.y >= outputDesc.DesktopCoordinates.top && cursorPos.y < outputDesc.DesktopCoordinates.bottom;
        if (!cursorInDisplay) {
            return false;
        }
        return true;
    }

    // Resize buffer if needed
    if (frameInfo->PointerShapeBufferSize > monitor->pointerInfo.shapeBuffer.size()) {
        monitor->pointerInfo.shapeBuffer.resize(frameInfo->PointerShapeBufferSize);
    }

    // Get shape
    UINT bufferSizeRequired = 0;
    HRESULT hr = deskDupl->GetFramePointerShape(
        frameInfo->PointerShapeBufferSize,
        reinterpret_cast<VOID*>(monitor->pointerInfo.shapeBuffer.data()), &bufferSizeRequired,
        &monitor->pointerInfo.shapeInfo);
    if (FAILED(hr)) {
        monitor->pointerInfo.shapeBuffer.clear();
        qCCritical(lcPointerMonitor, "Failed to get frame pointer shape");
        return false;
    }

    // Resize to actual required size
    monitor->pointerInfo.shapeBuffer.resize(bufferSizeRequired);

    size_t hash = 0;
    if (!monitor->pointerInfo.shapeBuffer.isEmpty()) {
        hash = qHash(monitor->pointerInfo.shapeBuffer);
        if (hash != monitor->pointerInfo.hash) {
            monitor->pointerInfo.hash = hash;
            monitor->pointerInfo.changed = true;
        }
    }

    return true;
}

DisplayDuplication::FrameReturn DisplayDuplication::getFrame(FrameInfo& frameInfoHolder) const
{
    ComPtr<IDXGIResource> desktopResource;

    // Get new frame
    HRESULT hr = deskDupl->AcquireNextFrame(0, frameInfoHolder.inner, desktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return FrameReturn::Timeout;
    }
    if (FAILED(hr)) {
        qCCritical(lcPointerMonitor, "Failed to acquire next frame");
        return FrameReturn::Failure;
    }

    frameInfoHolder.valid = true;

    return FrameReturn::Success;
}

bool PointerInfo::ConvertPointerShapeToQImage(QImage& image)
{
    if (width() == 0 || height() == 0 || shapeBuffer.isEmpty()) {
        return false;
    }

    switch (shapeInfo.Type) {
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR: {
        // The pointer type is a color mouse pointer, which is a color bitmap.
        // The bitmap's size is specified by width and height in a 32 bpp ARGB DIB format.
        image = QImage(reinterpret_cast<const uchar*>(shapeBuffer.constData()), static_cast<int>(width()), static_cast<int>(height()), static_cast<qsizetype>(pitch()), QImage::Format_ARGB32);
        return true;
    }

    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME: {
        // The pointer type is a monochrome mouse pointer, which is a monochrome bitmap.
        // The bitmap's size is specified by width and height in a 1 bits per pixel (bpp) device independent bitmap (DIB) format
        // AND mask that is followed by another 1 bpp DIB format
        // XOR mask of the same size.

        // MONO 缓冲区上半 AND，下半 XOR，所以 height = Height / 2
        // 每行中每像素占用 1 bit，每 8 像素占 1 字节，pitch 是每行字节数
        // 用 row * pitch 可算出当前行起始字节位置
        // 用 col / 8 可算出当前像素在当前行中是第几个字节
        // DXGI 的单色光标 bit 排序是：
        // bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0
        // mask = 0x80 >> (col % 8)  得到当前字节内的第几位
        // 判断 mask 字节，与当前字节，按位且运算后，结果为 1 代表当前位是 1，为 0 代表当前位是 0

        int realHeight = static_cast<int>(height()) / 2;
        image = QImage(static_cast<int>(width()), realHeight, QImage::Format_ARGB32);

        for (int row = 0; row < realHeight; ++row) {
            auto dst = reinterpret_cast<quint32*>(image.scanLine(row));

            for (quint32 col = 0; col < width(); ++col) {
                uint8_t mask = 0x80 >> (col % 8);
                // AND mask
                bool andBit = (shapeBuffer[row * pitch() + col / 8] & mask);
                // XOR mask
                bool xorBit = (shapeBuffer[(realHeight + row) * pitch() + col / 8] & mask);

                quint32 pixel = 0x00000000; // 默认 alpha = 255

                if (!andBit && !xorBit) {
                    // Case 1: AND=0 XOR=0 → 黑色
                    pixel = 0xFF000000;
                } else if (!andBit && xorBit) {
                    // Case 2: AND=0 XOR=1 → 白色
                    pixel = 0xFFFFFFFF;
                } else if (andBit && !xorBit) {
                    // Case 3: AND=1 XOR=0 → 显示桌面背景 => 算作透明
                    pixel = 0x00000000;
                } else { // (andBit && xorBit)
                    // Case 4: AND=1 XOR=1 → 反色 => 算作黑色
                    pixel = 0xFF000000;
                }

                dst[col] = pixel;
            }
        }

        return true;
    }

    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR: {
        // The pointer type is a masked color mouse pointer. A masked color mouse pointer is a 32 bpp ARGB format bitmap with the mask value in the alpha bits.
        // The only allowed mask values are 0 and 0xFF.
        // When the mask value is 0, the RGB value should replace the screen pixel.
        // When the mask value is 0xFF, an XOR operation is performed on the RGB value and the screen pixel; the result replaces the screen pixel.
        image = QImage(reinterpret_cast<const uchar*>(shapeBuffer.constData()), static_cast<int>(width()), static_cast<int>(height()), static_cast<qsizetype>(pitch()), QImage::Format_ARGB32);
        // 遍历每行每列修改 alpha 为 0xFF 的像素
        for (quint32 row = 0; row < height(); ++row) {
            auto dst = reinterpret_cast<quint32*>(image.scanLine(static_cast<int>(row)));
            for (quint32 col = 0; col < width(); ++col) {
                quint32 pixel = dst[col];
                uint8_t alpha = pixel >> 24;
                if (alpha == 0xFF) {
                    // alpha = 0xFF → 将 RGB 值与桌面值进行 XOR 运算
                    // 因为我们不处理反色，
                    // 如果 RGB 是黑色，则可认为是透明
                    // 其他情况认为是黑色
                    if ((pixel & 0x00FFFFFF) == 0x00000000) {
                        dst[col] = 0x00000000; // 透明
                    } else {
                        dst[col] = 0xFF000000; // 黑色
                    }
                } else if (alpha == 0x00) {
                    // alpha = 0x00 → 修改为不透明，保留 RGB
                    dst[col] = (pixel & 0x00FFFFFF) | 0xFF000000;
                } else {
                    qCInfo(lcPointerMonitor, "Alpha value unexpected: %u", alpha);
                    Q_ASSERT(false);
                    return false;
                }
            }
        }
        return true;
    }

    default:
        qCWarning(lcPointerMonitor, "Unknown pointer shape type: %u", shapeInfo.Type);
        return false;
    }
}

bool PointerInfo::SavePointerToPNG(const QString& filename)
{
    QImage image;
    if (!ConvertPointerShapeToQImage(image)) {
        return false;
    }

    // 保存为 PNG 文件
    bool saved = image.save(filename, "PNG");
    return saved;
}
