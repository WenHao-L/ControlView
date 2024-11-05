#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <opencv2/opencv.hpp>
#include <QMainWindow>
#include <QCloseEvent>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
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
#include "myGraphicsScene.h"

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

    void on_previewComboBox_currentIndexChanged(int index);

    void on_autoExposureCheckBox_stateChanged(int state);

    void on_exposureTargetSlider_valueChanged(int value);

    void on_exposureTimeSlider_valueChanged(int value);

    void on_gainSlider_valueChanged(int value);

    void on_autoAwbCheckBox_stateChanged(int state);

    void on_temperatureSlider_valueChanged(int value);

    void on_tintSlider_valueChanged(int value);

    void on_defaultAwbButton_clicked();

    void on_autoAbbCheckBox_stateChanged(int state);

    void on_redSlider_valueChanged(int value);

    void on_greenSlider_valueChanged(int value);

    void on_blueSlider_valueChanged(int value);

    void on_defaultAbbButton_clicked();

    void on_hueSlider_valueChanged(int value);

    void on_saturationSlider_valueChanged(int value);

    void on_brightnessSlider_valueChanged(int value);

    void on_contrastSlider_valueChanged(int value);

    void on_gammaSlider_valueChanged(int value);

    void on_defaultColorButton_clicked();

    void onAERectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio);

    void onAWBRectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio);

    void onABBRectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio);

    void on_unitComboBox_currentIndexChanged(int index);

    void addLineWidgets(QGraphicsLineItem* lineItem, QPointF startPoint, QPointF endPoint);

    void removeLineWidgets(QGraphicsLineItem* lineItem);

    void handleImageCaptured(const QImage &image);

    void handleStillImageCaptured(const QImage &image);

    void handleCameraStartMessage(bool message);

    void handleEventCallBackMessage(QString message);

    void closeTab(int index);

    // 串口
    void on_actionSerial_triggered(bool checked);

    void on_searchSerialButton_clicked();

    void on_openSerialButton_clicked();

    void on_lockButton_clicked();

    void on_bigShiftButton_clicked();

    void on_smallShiftSlider_valueChanged(int value);

    void readSerialData();

    void onSpeedChanged();

    void sendData();

    void handleSerialError(QSerialPort::SerialPortError error);

private:
    void resizeEvent(QResizeEvent *event);

    void openCamera();

    int closeCamera();

    void startCamera();

    // 串口
    void closeSerial();


    Ui::MainWindow*      ui;
    cv::VideoWriter      m_videoWriter;
    bool                 m_isRecording;
    NncamDeviceV2        m_cur;
    HNncam               m_hcam;
    QTimer*              m_timer;
    QTimer*              m_serialTimer;
    unsigned             m_imgWidth;
    unsigned             m_imgHeight;
    unsigned             m_previewWidth;
    unsigned             m_previewHeight;
    float                m_xpixsz;
    float                m_ypixsz;
    uchar*               m_pData;
    int                  m_res;
    int                  m_target;
    int                  m_time;
    int                  m_gain;
    int                  m_temp;
    int                  m_tint;
    unsigned short       m_aSub[3] = {0, 0, 0};
    unsigned short       m_red;
    unsigned short       m_green;
    unsigned short       m_blue;
    int                  m_hue;
    int                  m_saturation;
    int                  m_brightness;
    int                  m_contrast;
    int                  m_gamma;
    unsigned             m_count;
    MyGraphicsScene*     m_scene;
    QGraphicsView*       m_imageView;
    QGraphicsPixmapItem* m_pixmapItem;
    RectItem*            m_aeItem;
    RectItem*            m_awbItem;
    RectItem*            m_abbItem;
    cameraThread*        m_cameraThread;
    RECT                 m_aeRect;
    RECT                 m_awbRect;
    RECT                 m_abbRect;
    QVector<QImage>      imageVector;
    QMap<QGraphicsLineItem*, QLabel*>       labels;
    QMap<QGraphicsLineItem*, QPushButton*>  deleteButtons;
    QMap<QGraphicsLineItem*, QWidget*>      layoutWidgets;

    QByteArray rForwardData = QByteArray::fromHex("000000000000000102000200020000");  
    QByteArray rBackwardData = QByteArray::fromHex("00000000ffffffff02000200020000");

    QByteArray tForwardData = QByteArray::fromHex("000000010000000002000200020000");
    QByteArray tBackwardData = QByteArray::fromHex("ffffffff0000000002000200020000");

    QByteArray yForwardData = QByteArray::fromHex("0000000000000000020001ff020000");
    QByteArray yBackwardData = QByteArray::fromHex("000000000000000002000201020000");

    QByteArray xForwardData = QByteArray::fromHex("000000000000000001ff0200020000");
    QByteArray xBackwardData = QByteArray::fromHex("000000000000000002010200020000");

    QByteArray zForwardData = QByteArray::fromHex("00000000000000000200020001ff00");
    QByteArray zBackwardData = QByteArray::fromHex("000000000000000002000200020100");

    QByteArray bigShiftData = QByteArray::fromHex("000000000000000002000200020020");

    QByteArray defaultData = QByteArray::fromHex("000000000000000002000200020000");
    QByteArray sendDataPacket;
    QByteArray defaultDataPacket;

    float rOriginAngle = 0.0;
    float tOriginAngle = 0.0;
    int m_measureFlag = 0;
    float m_distance = 0.0;
    int m_bigShiftFlag = 0;
    int m_lockFlag = 0;

    // 串口通信
    QByteArray createPacket(const QByteArray &data);

    void processReceivedData(const QByteArray &data);

    QSerialPort* m_serial;
    
};
#endif // MAINWINDOW_H
