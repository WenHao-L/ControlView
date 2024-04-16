#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <nncam.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    NncamDeviceV2 mCur;
    HNncam        mHcam;
    QTimer*       mTimer;
    unsigned      mImgWidth;
    unsigned      mImgHeight;
    uchar*        mPData;
    int           mRes;
    int           mTemp;
    int           mTint;
    unsigned      mCount;

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

protected:
    void closeEvent(QCloseEvent*) override;

private slots:
    void on_captureButton_clicked();

    void on_videoButton_clicked();

    void on_whileBalanceButton_clicked();

    void on_defaultValueButton_clicked();

    void on_searchCameraButton_clicked();

    void on_cameraButton_clicked();

    void on_previewComboBox_currentIndexChanged(int index);

    void on_autoExposureCheckBox_stateChanged(bool state);

    void on_exposureTimeSlider_valueChanged(int value);

    void on_gainSlider_valueChanged(int value);

    void on_temperatureSlider_valueChanged(int value);

    void on_tintSlider_valueChanged(int value);

signals:
    void evtCallback(unsigned nEvent);

private:
    void openCamera();

    void closeCamera();

    void startCamera();

    void handleImageEvent();

    void handleExpoEvent();

    void handleTempTintEvent();

    void handleStillImageEvent();

    static void __stdcall eventCallBack(unsigned nEvent, void* pCallbackCtx);

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
