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
#include <QSerialPort>
#include "cameraThread.h"
#include "rectItem.h"

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

    void on_previewComboBox_currentIndexChanged(int index);

    void on_autoExposureCheckBox_stateChanged(int state);

    void on_exposureTimeSlider_valueChanged(int value);

    void on_gainSlider_valueChanged(int value);

    void on_temperatureSlider_valueChanged(int value);

    void on_tintSlider_valueChanged(int value);

    void on_defaultAwbButton_clicked();

    void on_hueSlider_valueChanged(int value);

    void on_saturationSlider_valueChanged(int value);

    void on_brightnessSlider_valueChanged(int value);

    void on_contrastSlider_valueChanged(int value);

    void on_gammaSlider_valueChanged(int value);

    void on_defaultColorButton_clicked();

    void handleImageCaptured(const QImage &image);

    void handleStillImageCaptured(const QImage &image);

    void handleCameraStartMessage(bool message);

    void handleEventCallBackMessage(QString message);

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


private:
    void openCamera();

    int closeCamera();

    void startCamera();

    void closeTab(int index);

    Ui::MainWindow*      ui;
    cv::VideoWriter      m_videoWriter;
    bool                 m_isRecording;
    NncamDeviceV2        m_cur;
    HNncam               m_hcam;
    QTimer*              m_timer;
    unsigned             m_imgWidth;
    unsigned             m_imgHeight;
    uchar*               m_pData;
    int                  m_res;
    int                  m_time;
    int                  m_gain;
    int                  m_temp;
    int                  m_tint;
    int                  m_hue;
    int                  m_saturation;
    int                  m_brightness;
    int                  m_contrast;
    int                  m_gamma;
    unsigned             m_count;
    QGraphicsScene*      m_scene;
    QGraphicsView*       m_imageView;
    QGraphicsPixmapItem* m_pixmapItem;
    RectItem*            m_exposureItem;
    RectItem*            m_awbItem;
    cameraThread*        m_cameraThread;
    QWidget*             previewTab;
    QGraphicsView*       previewView;
    QGraphicsScene*      previewScene;
    QPixmap              framePixmap;
    QVector<QImage>      imageVector;

    QByteArray rForwardData = QByteArray::fromHex("000000000000000100000000");
    QByteArray rBackwardData = QByteArray::fromHex("000000000000000100000000");
    QByteArray tForwardData = QByteArray::fromHex("000000010000000000000000");
    QByteArray tBackwardData = QByteArray::fromHex("000000010000000000000000");
    float rOriginAngle = 0.0;
    float tOriginAngle = 0.0;

    // 串口通信
    QByteArray createPacket(const QByteArray &data);

    uint16_t calculateCRC16(const QByteArray &data);

    void processReceivedData(const QByteArray &data);

    QSerialPort* m_serial;
    
};
#endif // MAINWINDOW_H
