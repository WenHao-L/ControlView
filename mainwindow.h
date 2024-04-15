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

private slots:
    void on_captureButton_clicked();

    void on_videoButton_clicked();

    void on_whileBalanceButton_clicked();

    void on_defaultValueButton_clicked();

    void on_searchCameraButton_clicked();

    void on_cameraButton_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
