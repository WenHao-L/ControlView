#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QString>
#include <nncam.h>
#include <wmv.h>

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
    void updateMainStyle(QString style);

    void initPreview();

    void openCamera();

    int closeCamera();

    void startCamera();

    void handleImageEvent();

    void handleExpoEvent();

    void handleTempTintEvent();

    void handleStillImageEvent();

    void closeTab(int index);

    void stopRecord();

    static void __stdcall eventCallBack(unsigned nEvent, void* pCallbackCtx);

    Ui::MainWindow *ui;
    CWmvRecord*	  m_pWmvRecord;
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
    
};
#endif // MAINWINDOW_H
