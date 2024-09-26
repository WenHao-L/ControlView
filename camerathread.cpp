#include "cameraThread.h"

cameraThread::cameraThread(HNncam hcam, uchar* pData, QObject *parent)
    : QThread(parent), hcam(hcam), pData(pData)
{
}

cameraThread::~cameraThread()
{
}

void cameraThread::run()
{
    if (SUCCEEDED(Nncam_StartPullModeWithCallback(hcam, eventCallBack, this)))
    {
        emit cameraStartMessage(true);
    }
    else
    {
        emit cameraStartMessage(false);
    }
}

void __stdcall cameraThread::eventCallBack(unsigned nEvent, void* pCallbackCtx)
{
    cameraThread* pThis = reinterpret_cast<cameraThread*>(pCallbackCtx);
    if (pThis->hcam)
        {
            if (NNCAM_EVENT_IMAGE == nEvent)
                pThis->handleImageEvent();
            else if (NNCAM_EVENT_STILLIMAGE == nEvent)
                pThis->handleStillImageEvent();
            else if (NNCAM_EVENT_ERROR == nEvent)
            {
                emit pThis->eventCallBackMessage("一般性错误, 数据采集不能继续。");
            }
            else if (NNCAM_EVENT_DISCONNECTED == nEvent)
            {
                emit pThis->eventCallBackMessage("相机断开连接。");
            }
        }
}

void cameraThread::handleImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Nncam_PullImage(hcam, pData, 24, &width, &height)))
    {
        QImage image(pData, width, height, QImage::Format_RGB888);
        emit imageCaptured(image);
    }
}

void cameraThread::handleStillImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Nncam_PullStillImage(hcam, nullptr, 24, &width, &height))) // peek
    {
        std::vector<uchar> vec(TDIBWIDTHBYTES(width * 24) * height);
        if (SUCCEEDED(Nncam_PullStillImage(hcam, &vec[0], 24, &width, &height)))
        {
            QImage image(&vec[0], width, height, QImage::Format_RGB888);
            QImage copiedImage = image.copy();
            emit stillImageCaptured(copiedImage);
        }
    }
}