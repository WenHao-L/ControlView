#ifndef CAMERATHREAD_H
#define CAMERATHREAD_H
#include <QThread>
#include <QImage>
#include <QString>
#include "Nncam.h"

class cameraThread : public QThread
{
    Q_OBJECT

public:
    cameraThread(HNncam hcam, uchar* pData, QObject *parent = nullptr);
    ~cameraThread();
    void run() override;

    signals:
        void imageCaptured(const QImage &image);
        void stillImageCaptured(const QImage &image);
        void cameraStartMessage(bool Message);
        void eventCallBackMessage(QString Message);
    
    private:
        HNncam hcam;
        uchar* pData;

        static void __stdcall eventCallBack(unsigned nEvent, void* pCallbackCtx);

        void handleImageEvent();

        void handleStillImageEvent();
};

#endif // CAMERATHREAD_H
