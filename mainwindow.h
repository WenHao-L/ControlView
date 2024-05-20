#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <opencv2/opencv.hpp>
#include <QMainWindow>
#include <QCloseEvent>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QString>
#include <nncam.h>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QByteArray>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_searchCameraButton_clicked();

    void on_cameraButton_clicked();

    void on_captureButton_clicked();

    void on_videoButton_clicked();

    void on_whileBalanceButton_clicked();

    void on_defaultValueButton_clicked();

    void on_previewComboBox_currentIndexChanged(int index);

    void on_autoExposureCheckBox_stateChanged(int state);

    void on_exposureTimeSlider_valueChanged(int value);

    void on_gainSlider_valueChanged(int value);

    void on_temperatureSlider_valueChanged(int value);

    void on_tintSlider_valueChanged(int value);

    // 串口
    void on_actionSerial_triggered(bool checked);

    void on_searchSerialButton_clicked();

    void on_openSerialButton_clicked();

    void readSerialData();

    void on_rAxisForwardButton_clicked();

    void on_rAxisBackwardButton_clicked();

    void on_tAxisForwardButton_clicked();

    void on_tAxisBackwardButton_clicked();

    void onSpeedChanged();

signals:
    void evtCallback(unsigned nEvent);

private:
    void initPreview();

    void openCamera();

    int closeCamera();

    void startCamera();

    void handleImageEvent();

    void handleExpoEvent();

    void handleTempTintEvent();

    void handleStillImageEvent();

    void closeTab(int index);

    static void __stdcall eventCallBack(unsigned nEvent, void* pCallbackCtx);

    Ui::MainWindow *ui;
    cv::VideoWriter m_videoWriter;
    bool m_isRecording;
    NncamDeviceV2 m_cur;
    HNncam        m_hcam;
    QTimer*       m_timer;
    unsigned      m_imgWidth;
    unsigned      m_imgHeight;
    uchar*        m_pData;
    int           m_res;
    int           m_temp;
    int           m_tint;
    unsigned      m_count;
    QWidget*             previewTab;
    QGraphicsView*       previewView;
    QGraphicsScene*      previewScene;
    QPixmap              framePixmap;
    QGraphicsPixmapItem* pixmapItem;
    QVector<QImage>      imageVector;

    // 串口通信
    QByteArray createPacket(const QByteArray &data);

    uint16_t calculateCRC16(const QByteArray &data);

    void processReceivedData(const QByteArray &data);

    QSerialPort* m_serial;
    
};
#endif // MAINWINDOW_H
