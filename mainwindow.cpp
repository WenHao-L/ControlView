#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "crc16.h"
//#include "CustomTitleBar.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isRecording(false)
    , m_hcam(nullptr)
    , m_timer(new QTimer(this))
    , m_imgWidth(0), m_imgHeight(0), m_pData(nullptr)
    , m_res(0), m_temp(NNCAM_TEMP_DEF), m_tint(NNCAM_TINT_DEF), m_count(0)
    , m_serial(new QSerialPort(this))
{
    ui->setupUi(this);
    // setWindowFlags(Qt::FramelessWindowHint);

    // 仪表盘设置
    ui->rGaugePanelWidget->setValueRange(60.0);
    ui->tGaugePanelWidget->setValueRange(200.0);
    ui->rGaugePanelWidget->setValueStep(1.0);
    ui->tGaugePanelWidget->setValueStep(1.0);
    ui->rGaugePanelWidget->setValue(0.0);
    ui->tGaugePanelWidget->setValue(0.0);

    QFile qss(":qdarkstyle/dark/darkstyle.qss");

    if(qss.open(QFile::ReadOnly))
    {
        QTextStream filetext(&qss);
        QString stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    }

    // 微控制器可视化
    ui->serialWidget->setVisible(true);

    // 设置默认白平衡参数
    on_defaultAwbButton_clicked();

    // 设置默认颜色调整参数
    on_defaultColorButton_clicked();

    // 定时器更新帧率显示
    connect(m_timer, &QTimer::timeout, this, [this]()
    {
        unsigned nFrame = 0, nTime = 0, nTotalFrame = 0;
        if (m_hcam && SUCCEEDED(Nncam_get_FrameRate(m_hcam, &nFrame, &nTime, &nTotalFrame)) && (nTime > 0))
            ui->lblLabel->setText(QString::asprintf("%u, fps = %.1f", nTotalFrame, nFrame * 1000.0 / nTime));
    });

    // 连接tab关闭信号和槽
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::closeTab);
    connect(ui->lowSpeedRadioButton, &QRadioButton::clicked, this, &MainWindow::onSpeedChanged);
    connect(ui->mediumSpeedRadioButton, &QRadioButton::clicked, this, &MainWindow::onSpeedChanged);
    connect(ui->highSpeedRadioButton, &QRadioButton::clicked, this, &MainWindow::onSpeedChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_hcam)
    {
        int closeRes = closeCamera();
        if (closeRes == 1)
        {
            delete m_timer;
            m_timer = nullptr;
        }
        else
        {
            event->ignore();
            return;
        }
    }
    else
    {
        delete m_timer;
        m_timer = nullptr;
    }

    if (m_serial)
    {
        if (m_serial->isOpen())  //原先串口打开，则关闭串口
            m_serial->close();
        delete[] m_serial;
        m_serial = nullptr;
    }
}

void MainWindow::on_searchCameraButton_clicked()
{
    // 如果相机已打开，则关闭相机
    if (m_hcam)
    {
        closeCamera();
    }
    else
    {
        // 清空相机列表
        ui->cameraComboBox->clear();
        // 枚举可用相机设备
        NncamDeviceV2 arr[NNCAM_MAX] = { 0 };
        unsigned count = Nncam_EnumV2(arr);
        // 如果没有找到相机，则显示警告信息
        if (0 == count)
            QMessageBox::warning(this, "Warning", u8"没有找到相机。");
        // 如果找到一个相机，则获取该相机并添加到相机列表
        else if (1 == count)
        {
            m_cur = arr[0];
            ui->cameraComboBox->addItem(QString::fromWCharArray(arr[0].displayname));
            ui->cameraComboBox->setEnabled(true);
            ui->cameraButton->setEnabled(true);
        }
        // 如果找到多个相机，则获取所有相机并添加到相机列表，获取第一个相机
        else
        {
            for (unsigned i = 0; i < count; ++i)
            {
                ui->cameraComboBox->addItem(QString::fromWCharArray(arr[i].displayname));
            }
            m_cur = arr[0];
            ui->cameraComboBox->setCurrentIndex(0);
            ui->cameraComboBox->setEnabled(true);
            ui->cameraButton->setEnabled(true);
        }
    }
}

void MainWindow::on_cameraButton_clicked()
{
    if (ui->cameraButton->text() == "打开相机")
        openCamera();
    else
        closeCamera();
}

void MainWindow::on_captureButton_clicked()
{
    if (m_hcam)
    {
        if (0 == m_cur.model->still)    // not support still image capture
        {
            if (m_pData)
            {
                QImage image(m_pData, m_imgWidth, m_imgHeight, QImage::Format_RGB888);

                // 创建一个新的标签页
                QWidget *newTab = new QWidget();
                QLabel *imageLabel = new QLabel(newTab);

                // 设置标签页的布局，确保QLabel自适应标签页大小
                QVBoxLayout *layout = new QVBoxLayout(newTab);
                layout->addWidget(imageLabel);
                layout->setContentsMargins(0, 0, 0, 0);
                newTab->setLayout(layout);

                QPixmap pixmap = QPixmap::fromImage(image).scaled(newTab->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->setScaledContents(true);
                imageLabel->setPixmap(pixmap);

                // 将新创建的QImage存储到vetor中，并添加标签页到tabWidget
                ui->tabWidget->addTab(newTab, QString("image_") + QString::number(++m_count));
                imageVector.append(image);
            }
        }
        else
        {
            qDebug() << "still\r\n";
            int currentCaptureIndex = ui->captureComboBox->currentIndex();
        
            // 使用当前选中的序号作为参数调用 Nncam_Snap 函数
            Nncam_Snap(m_hcam, currentCaptureIndex);
        }
    }
}

void MainWindow::on_videoButton_clicked()
{
    if (ui->videoButton->text() == "录像")
    {
        // 确保相机已经打开，并且有有效的图像数据
        if (!m_hcam)
        {
            QMessageBox::warning(this, tr("Record Error"), tr("Camera is not open."));
            return;
        }

        QString videoFileName = QFileDialog::getSaveFileName(this, tr("Save Video"), "", tr("Video Files (*.mp4)"));
        if (!videoFileName.isEmpty())
        {
            double fps = 30.0;
            m_videoWriter.open(videoFileName.toStdString(), cv::VideoWriter::fourcc('X','2','6','4'), fps, cv::Size(m_imgWidth, m_imgHeight), true);

            if (m_videoWriter.isOpened())
            {
                ui->videoButton->setText("停止录像");
                m_isRecording = true;
            }
            else
            {
                QMessageBox::warning(this, tr("Record Error"), tr("Failed to start recording."));
            }
        }
    }
    else
    {
        // 停止录制
        if (m_isRecording)
        {
            m_videoWriter.release();
            ui->videoButton->setText("录像");
            m_isRecording = false;
        }
    }
}

void MainWindow::on_whileBalanceButton_clicked()
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_AwbOnce(m_hcam, nullptr, nullptr)))
        {
        }
        else
        {
            QMessageBox::warning(this, "Warning", u8"自动白平衡调整失败。"); 
        }
    }
}

void MainWindow::on_previewComboBox_currentIndexChanged(int index)
{
    if (m_hcam)
        Nncam_Stop(m_hcam);

    m_res = index;
    m_imgWidth = m_cur.model->res[index].width;
    m_imgHeight = m_cur.model->res[index].height;

    if (m_hcam)
    {
        Nncam_put_eSize(m_hcam, static_cast<unsigned>(m_res));
        startCamera();
    }
}

void MainWindow::on_autoExposureCheckBox_stateChanged(int state)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_AutoExpoEnable(m_hcam, state ? 1 : 0)))
        {
            ui->exposureTimeSlider->setEnabled(!state);
            ui->gainSlider->setEnabled(!state);
        }
        else
        {
            // 获取当前自动曝光的状态，并设置复选框的选中状态
            int bAuto = 0;
            Nncam_get_AutoExpoEnable(m_hcam, &bAuto);
            ui->exposureTimeSlider->setEnabled(!(1 == bAuto));
            ui->gainSlider->setEnabled(!(1 == bAuto));
            {
                const QSignalBlocker blocker(ui->autoExposureCheckBox);
                ui->autoExposureCheckBox->setChecked(1 == bAuto);
            }
            QMessageBox::warning(this, "Warning", u8"自动曝光设置失败。"); 
        }
    }
}

void MainWindow::on_exposureTimeSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoExposureCheckBox->isChecked())
            if (SUCCEEDED(Nncam_put_ExpoTime(m_hcam, static_cast<unsigned>(value))))
            {
                m_time = value;
                ui->exposureTargetNumLabel->setText(QString::number(value));
            }
            else
            {
                {
                    const QSignalBlocker blocker(ui->exposureTimeSlider);
                    ui->exposureTimeSlider->setValue(m_time);
                }
                QMessageBox::warning(this, "Warning", u8"调整曝光时间失败。"); 
            }
    }
}

void MainWindow::on_gainSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoExposureCheckBox->isChecked())
            if (SUCCEEDED(Nncam_put_ExpoAGain(m_hcam, static_cast<unsigned short>(value))))
            {
                m_gain = value;
                ui->gainNumLabel->setText(QString::number(value));
            }
            else
            {
                {
                    const QSignalBlocker blocker(ui->gainSlider);
                    ui->gainSlider->setValue(m_gain);
                }
                QMessageBox::warning(this, "Warning", u8"调整曝光增益失败。"); 
            }
    }
}

void MainWindow::on_temperatureSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_TempTint(m_hcam, value, m_tint)))
        {
            m_temp = value;
            ui->temperatureNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->temperatureSlider);
                ui->temperatureSlider->setValue(m_temp);
            }
            QMessageBox::warning(this, "Warning", u8"调整色温失败。"); 
        }
    }
}

void MainWindow::on_tintSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_TempTint(m_hcam, m_temp, value)))
        {
            m_tint = value;
            ui->tintNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->tintSlider);
                ui->tintSlider->setValue(m_temp);
            }
            QMessageBox::warning(this, "Warning", u8"调整Tint失败。");          
        }
    }
}

void MainWindow::on_defaultAwbButton_clicked()
{
    // 设置白平衡默认参数
    {
        const QSignalBlocker blocker(ui->temperatureSlider);
        ui->temperatureNumLabel->setText(QString::number(NNCAM_TEMP_DEF));
        ui->temperatureSlider->setRange(NNCAM_TEMP_MIN, NNCAM_TEMP_MAX);
        ui->temperatureSlider->setValue(NNCAM_TEMP_DEF);
    }
    {
        const QSignalBlocker blocker(ui->tintSlider);
        ui->tintNumLabel->setText(QString::number(NNCAM_TINT_DEF));
        ui->tintSlider->setRange(NNCAM_TINT_MIN, NNCAM_TINT_MAX);
        ui->tintSlider->setValue(NNCAM_TINT_DEF);
    }
}

void MainWindow::on_hueSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_Hue(m_hcam, value)))
        {
            m_hue = value;
            ui->hueNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->hueSlider);
                ui->hueSlider->setValue(m_hue);
            }
            QMessageBox::warning(this, "Warning", u8"调整色度失败。");          
        }
    }
}

void MainWindow::on_saturationSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_Saturation(m_hcam, value)))
        {
            m_saturation = value;
            ui->saturationNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->saturationSlider);
                ui->saturationSlider->setValue(m_saturation);
            }
            QMessageBox::warning(this, "Warning", u8"调整饱和度失败。");          
        }
    }
}

void MainWindow::on_brightnessSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_Brightness(m_hcam, value)))
        {
            m_brightness = value;
            ui->brightnessNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->brightnessSlider);
                ui->brightnessSlider->setValue(m_brightness);
            }
            QMessageBox::warning(this, "Warning", u8"调整亮度失败。");          
        }
    }
}

void MainWindow::on_contrastSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_Contrast(m_hcam, value)))
        {
            m_contrast = value;
            ui->contrastNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->contrastSlider);
                ui->contrastSlider->setValue(m_contrast);
            }
            QMessageBox::warning(this, "Warning", u8"调整对比度失败。");          
        }
    }
}

void MainWindow::on_gammaSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_Gamma(m_hcam, value)))
        {
            m_gamma = value;
            ui->gammaNumLabel->setText(QString::number(value));
        }
        else
        {
            {
                const QSignalBlocker blocker(ui->gammaSlider);
                ui->gammaSlider->setValue(m_gamma);
            }
            QMessageBox::warning(this, "Warning", u8"调整Gamma失败。");          
        }
    }
}

void MainWindow::on_actionSerial_triggered(bool checked)
{
    ui->serialWidget->setVisible(checked);
}

void MainWindow::on_defaultColorButton_clicked()
{
    // 设置颜色默认参数
    {
        const QSignalBlocker blocker(ui->hueSlider);
        ui->hueNumLabel->setText(QString::number(0));
        ui->hueSlider->setRange(-180, 180);
        ui->hueSlider->setValue(0);
    }
    {
        const QSignalBlocker blocker(ui->saturationSlider);
        ui->saturationNumLabel->setText(QString::number(128));
        ui->saturationSlider->setRange(0, 255);
        ui->saturationSlider->setValue(128);
    }
    {
        const QSignalBlocker blocker(ui->brightnessSlider);
        ui->brightnessNumLabel->setText(QString::number(0));
        ui->brightnessSlider->setRange(-128, 128);
        ui->brightnessSlider->setValue(0);
    }
    {
        const QSignalBlocker blocker(ui->contrastSlider);
        ui->contrastNumLabel->setText(QString::number(0));
        ui->contrastSlider->setRange(-150, 150);
        ui->contrastSlider->setValue(0);
    }
    {
        const QSignalBlocker blocker(ui->gammaSlider);
        ui->gammaNumLabel->setText(QString::number(100));
        ui->gammaSlider->setRange(20, 180);
        ui->gammaSlider->setValue(100);
    }
}

void MainWindow::openCamera()
{
    // 打开摄像头
    m_hcam = Nncam_Open(m_cur.id);
    if (m_hcam)
    {
        // 设置帧速率级别
        unsigned short maxSpeed = Nncam_get_MaxSpeed(m_hcam);
        Nncam_put_Speed(m_hcam, maxSpeed);

        // 设置为RGB字节序（0：RGB，1：BGR），因为QImage使用RGB字节序
        Nncam_put_Option(m_hcam, NNCAM_OPTION_BYTEORDER, 0);

        // 设置为视频画面不倒置
        Nncam_put_Option(m_hcam, NNCAM_OPTION_UPSIDE_DOWN, 0);

        // 设置是否启用自动曝光
        Nncam_put_AutoExpoEnable(m_hcam, ui->autoExposureCheckBox->isChecked()? 1 : 0);

        // 获取摄像头的分辨率信息
        Nncam_get_eSize(m_hcam, (unsigned*)&m_res);

        // 获取当前分辨率下的图像宽度和高度
        m_imgWidth = m_cur.model->res[m_res].width;
        m_imgHeight = m_cur.model->res[m_res].height;

        // 更新previewComboBox、captureComboBox的下拉列表组件（分辨率）
        // 创建信号阻止器对象，用于暂时阻止ui->previewComboBox、ui->captureComboBox的信号发送
        {
            const QSignalBlocker blocker(ui->previewComboBox);
            ui->previewComboBox->clear();
            for (unsigned i = 0; i < m_cur.model->preview; ++i)
            {
                ui->previewComboBox->addItem(QString::asprintf("%u*%u", m_cur.model->res[i].width, m_cur.model->res[i].height));
            }
            ui->previewComboBox->setCurrentIndex(m_res);
            ui->previewComboBox->setEnabled(true);
        }
        {
            const QSignalBlocker blocker(ui->captureComboBox);
            ui->captureComboBox->clear();
            for (unsigned i = 0; i < m_cur.model->preview; ++i)
            {
                ui->captureComboBox->addItem(QString::asprintf("%u*%u", m_cur.model->res[i].width, m_cur.model->res[i].height));
            }
            ui->captureComboBox->setCurrentIndex(m_res);
            ui->captureComboBox->setEnabled(true);
        }

        // 初始化曝光时间范围及默认值
        unsigned uimax = 0, uimin = 0, uidef = 0;
        if (SUCCEEDED(Nncam_get_ExpTimeRange(m_hcam, &uimin, &uimax, &uidef)))
        {
            ui->exposureTimeSlider->setRange(uimin, uimax);
        }
        unsigned time = 0;
        if (SUCCEEDED(Nncam_get_ExpoTime(m_hcam, &time)))
        {
            {
                const QSignalBlocker blocker(ui->exposureTimeSlider);
                ui->exposureTimeSlider->setValue(int(time));
                ui->exposureTimeNumLabel->setText(QString::number(time));
            }
        }

        // 初始化曝光增益范围及默认值
        unsigned short usmax = 0, usmin = 0, usdef = 0;
        if (SUCCEEDED(Nncam_get_ExpoAGainRange(m_hcam, &usmin, &usmax, &usdef)))
        {
            ui->gainSlider->setRange(usmin, usmax);
        }
        unsigned short gain = 0;
        if (SUCCEEDED(Nncam_get_ExpoAGain(m_hcam, &gain)))
        {
            {
                const QSignalBlocker blocker(ui->gainSlider);
                ui->gainSlider->setValue(int(gain));
                ui->gainNumLabel->setText(QString::number(gain));
            }
        }

        // 如果当前模型不是单色相机，则处理温度和色度事件
        if (0 == (m_cur.model->flag & NNCAM_FLAG_MONO))
        {
            int nTemp = 0, nTint = 0;
            if (SUCCEEDED(Nncam_get_TempTint(m_hcam, &nTemp, &nTint)))
            {
                {
                    const QSignalBlocker blocker(ui->temperatureSlider);
                    ui->temperatureSlider->setValue(nTemp);
                    ui->temperatureNumLabel->setText(QString::number(nTemp));
                }
                {
                    const QSignalBlocker blocker(ui->tintSlider);
                    ui->tintSlider->setValue(nTint);
                    ui->tintNumLabel->setText(QString::number(nTint));
                }
            }
        }

        // 启动摄像头
        startCamera();
    }
}

int MainWindow::closeCamera()
{
    // 检查是否有未保存的预览图像
    bool hasUnsavedImages = imageVector.size() > 0;

    if (hasUnsavedImages)
    {
        // 弹出询问对话框，询问用户是否保存图像
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存图像",
            "您有未保存的预览图像，是否保存？",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes)
        {
            // 用户选择保存图像
            for (int i = 0; i < imageVector.size(); ++i)
            {
                QImage image = imageVector.at(i);
                if (!image.isNull())
                {
                    // 弹出保存对话框，让用户选择保存路径
                    QString filename = QString::asprintf("image_%u.jpg", i);
                    QString path = QFileDialog::getSaveFileName(this, "Save Image", filename, "JPEG Files (*.jpg *.jpeg)");

                    // 如果用户没有取消，保存图像
                    if (!path.isEmpty())
                    {
                        image.save(path);
                    }
                    else
                    {
                        // 如果用户取消保存，取消关闭行为
                        return 0;
                    }
                }

            }
        }
        else if (reply == QMessageBox::Cancel)
        {
            // 用户选择取消，不关闭相机
            return 0;
        }
    }

    if (m_cameraThread)
    {
        delete m_cameraThread;
        m_cameraThread = nullptr;
    }

    // 如果没有图像保存或者用户选择不保存，继续关闭相机并进行内存回收`
    if (m_hcam)
    {
        Nncam_Close(m_hcam);
        m_hcam = nullptr;
    }
    delete[] m_pData;
    m_pData = nullptr;

    if (m_isRecording)
    {
        m_videoWriter.release();
        ui->videoButton->setText("录像");
        m_isRecording = false;
    }

    // 清空图像向量
    imageVector.clear();

    ui->imageView->clear();
    // 删除预览标签页相关的QWidget对象
    // delete previewTab;
    // previewTab = nullptr;
    // delete previewView;
    // previewView = nullptr;
    // delete previewScene;
    // previewScene = nullptr;

    // 移除所有标签页
    while (ui->tabWidget->count() > 1)
    {
        ui->tabWidget->removeTab(1);
    }

    m_timer->stop();

    ui->lblLabel->clear();

    ui->cameraButton->setText("打开相机");
    ui->searchCameraButton->setEnabled(true);

    ui->captureButton->setEnabled(false);
    ui->videoButton->setEnabled(false);
    ui->previewComboBox->setEnabled(false);
    ui->previewComboBox->clear();
    ui->captureComboBox->setEnabled(false);
    ui->captureComboBox->clear();

    ui->autoExposureCheckBox->setEnabled(false);
    ui->exposureTargetSlider->setEnabled(false);
    ui->exposureTimeSlider->setEnabled(false);
    ui->gainSlider->setEnabled(false);

    ui->whileBalanceButton->setEnabled(false);
    ui->defaultAwbButton->setEnabled(false);
    ui->temperatureSlider->setEnabled(false);
    ui->tintSlider->setEnabled(false);

    ui->hueSlider->setEnabled(false);
    ui->saturationSlider->setEnabled(false);
    ui->brightnessSlider->setEnabled(false);
    ui->contrastSlider->setEnabled(false);
    ui->gammaSlider->setEnabled(false);
    ui->defaultColorButton->setEnabled(false);

    return 1;
}

void MainWindow::startCamera()
{
    // 检查mPData指针是否有效，如果是，则释放内存
    if (m_pData)
    {
        delete[] m_pData;
        m_pData = nullptr;
    }
    // 重新分配内存用于存储图像数据，TDIBWIDTHBYTES是一个宏，用于计算图像宽度所需的字节数
    m_pData = new uchar[TDIBWIDTHBYTES(m_imgWidth * 24) * m_imgHeight];

    m_cameraThread = new cameraThread(m_hcam, m_pData, this);
    connect(m_cameraThread, &cameraThread::imageCaptured, this, &MainWindow::handleImageCaptured);
    connect(m_cameraThread, &cameraThread::stillImageCaptured, this, &MainWindow::handleStillImageCaptured);
    connect(m_cameraThread, &cameraThread::cameraStartMessage, this, &MainWindow::handleCameraStartMessage);
    connect(m_cameraThread, &cameraThread::eventCallBackMessage, this, &MainWindow::handleEventCallBackMessage);
    m_cameraThread->start();
}

void MainWindow::handleImageCaptured(const QImage &image)
{
    QImage newimage = image.scaled(ui->previewTab->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    ui->imageView->setPixmap(QPixmap::fromImage(newimage));
    ui->imageView->setScaledContents(true);

    if (m_isRecording)
    {
        cv::Mat mat = cv::Mat(image.height(), image.width(), CV_8UC3, const_cast<uchar*>(image.bits()), image.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        m_videoWriter.write(mat);
    }
}

void MainWindow::handleStillImageCaptured(const QImage &image)
{
        // 创建一个新的标签页
        QWidget *newTab = new QWidget();
        QLabel *imageLabel = new QLabel(newTab);

        // 设置标签页的布局，确保QLabel自适应标签页大小
        QVBoxLayout *layout = new QVBoxLayout(newTab);
        layout->addWidget(imageLabel);
        layout->setContentsMargins(0, 0, 0, 0);
        newTab->setLayout(layout);

        QPixmap pixmap = QPixmap::fromImage(image).scaled(newTab->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imageLabel->setScaledContents(true);
        imageLabel->setPixmap(pixmap);

        // 将新创建的QImage存储到映射中，并添加标签页到tabWidget
        ui->tabWidget->addTab(newTab, QString("image_") + QString::number(++m_count));
        imageVector.append(image);
}

void MainWindow::handleCameraStartMessage(bool message)
{
    if (message)
    {
        // 修改相机开关按钮
        ui->cameraButton->setText("关闭相机");
        ui->searchCameraButton->setEnabled(false);

        // 使能捕获与分辨率功能
        ui->captureButton->setEnabled(true);
        ui->videoButton->setEnabled(true);
        ui->previewComboBox->setEnabled(true);
        ui->captureComboBox->setEnabled(true);
        ui->autoExposureCheckBox->setEnabled(true);

        // 使能曝光功能
        int bAuto = 0;
        Nncam_get_AutoExpoEnable(m_hcam, &bAuto);
        ui->exposureTimeSlider->setEnabled(!(1 == bAuto));
        ui->gainSlider->setEnabled(!(1 == bAuto));
        {
            const QSignalBlocker blocker(ui->autoExposureCheckBox);
            ui->autoExposureCheckBox->setChecked(1 == bAuto);
        }

        // 使能白平衡功能
        ui->whileBalanceButton->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->defaultAwbButton->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->temperatureSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->tintSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));

        // 使能颜色调整功能
        ui->hueSlider->setEnabled(true);
        ui->saturationSlider->setEnabled(true);
        ui->brightnessSlider->setEnabled(true);
        ui->contrastSlider->setEnabled(true);
        ui->gammaSlider->setEnabled(true);
        ui->defaultColorButton->setEnabled(true);

        // 启动一个定时器，每秒触发一次
        m_timer->start(1000);
    }
    else
    {
        closeCamera();
        QMessageBox::warning(this, "Warning", u8"无法打开相机。");   
    }
}

void MainWindow::handleEventCallBackMessage(QString message)
{
    closeCamera();
    QMessageBox::warning(this, "Warning", message);
}

void MainWindow::closeTab(int index)
{
    if (index == 0)
        closeCamera();

    else if (index > 0 && index < ui->tabWidget->count() && index <= imageVector.size())
    {
        QImage image = imageVector.at(index-1);

        QString filename = QString::asprintf("image_%u.jpg", index-1);
        QString path = QFileDialog::getSaveFileName(this, "Save Image", filename, "JPEG Files (*.jpg *.jpeg)");

        // 检查用户是否取消了对话框
        if (!path.isEmpty())
        {
            // 用户选择了保存路径，保存图像
            image.save(path);

            // 从vector中移除对应的QImage
            imageVector.removeAt(index-1);
            ui->tabWidget->removeTab(index);
            QWidget *widget = ui->tabWidget->widget(index);
            widget->deleteLater();
        }
    }
}

void MainWindow::on_searchSerialButton_clicked()
{
    if (m_serial)
    {
        if (m_serial->isOpen())  //原先串口打开，则关闭串口
            m_serial->close();
        delete[] m_serial;
        m_serial = nullptr;
    }

    //先清除所有串口列表
    ui->portBox->clear();

    //光标移动到结尾
    ui->serialMessageEdit->moveCursor(QTextCursor::End);
    ui->serialMessageEdit->insertPlainText(u8"\r\n串口初始化：\r\n");


    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        m_serial = new QSerialPort;
        m_serial->setPort(info);

        if(m_serial->open(QIODevice::ReadWrite))
        {
            ui->serialMessageEdit->insertPlainText(u8"可用："+m_serial->portName()+"\r\n");
            ui->portBox->addItem(m_serial->portName());
            m_serial->close();
        }
        else
        {
            ui->serialMessageEdit->insertPlainText(u8"不可用："+m_serial->portName()+"\r\n");
        }
    }

    ui->serialMessageEdit->moveCursor(QTextCursor::End);        //光标移动到结尾

    if (ui->portBox->count() > 0)
        ui->openSerialButton->setEnabled(true);
}

void MainWindow::on_openSerialButton_clicked()
{
    //尝试打开串口
    if(ui->openSerialButton->text() == tr(u8"打开串口"))
    {
        if(ui->portBox->currentText() == "" )
        {
            QMessageBox::warning(NULL, u8"警告", u8"无可开启串口！\r\n\r\n");
            return;
        }

        m_serial = new QSerialPort;
        //设置串口名
        m_serial->setPortName(ui->portBox->currentText());
        //设置波特率
        m_serial->setBaudRate(QSerialPort::Baud115200);
        //设置数据位
        m_serial->setDataBits(QSerialPort::Data8);
        //设置校验位
        m_serial->setParity(QSerialPort::NoParity);
        //设置停止位
        m_serial->setStopBits(QSerialPort::OneStop);
        //设置流控制
        m_serial->setFlowControl(QSerialPort::NoFlowControl);  //设置为无流控制

        //连接信号和槽函数，串口有数据可读时，调用ReadData()函数读取数据并处理。
        connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);

        //打开串口
        if (!m_serial->open(QIODevice::ReadWrite)) {
            QMessageBox::warning(NULL, u8"警告", u8"串口打开失败！\r\n");
            return;
        }

        //使能
        ui->portBox->setEnabled(false);
        ui->openSerialButton->setText(u8"关闭串口");
        ui->openSerialButton->setStyleSheet("background-color:red");
        ui->highSpeedRadioButton->setEnabled(true);
        ui->mediumSpeedRadioButton->setEnabled(true);
        ui->lowSpeedRadioButton->setEnabled(true);
        ui->rAxisForwardButton->setEnabled(true);
        ui->rAxisBackwardButton->setEnabled(true);
        ui->tAxisForwardButton->setEnabled(true);
        ui->tAxisBackwardButton->setEnabled(true);

        ui->serialMessageEdit->insertPlainText(u8"串口连接成功！\r\n");
    }
    else
    {
        if (m_serial->isOpen())  //原先串口打开，则关闭串口
            m_serial->close();

        //释放串口
        delete[] m_serial;
        m_serial = nullptr;

        //恢复使能
        ui->portBox->setEnabled(true);
        ui->openSerialButton->setText(u8"打开串口");
        ui->openSerialButton->setStyleSheet("");
    }
}

QByteArray MainWindow::createPacket(const QByteArray &data) {
    QByteArray packet;
    packet.append(0x55);  // Start byte
    packet.append(data);

    uint16_t crc = calculateCRC16(data);
    packet.append(reinterpret_cast<const char*>(&crc), sizeof(crc));

    packet.append(0xAA);  // End byte
    return packet;
}

void MainWindow::readSerialData()
{
    QByteArray data = m_serial->readAll();
    processReceivedData(data);
}

uint16_t MainWindow::calculateCRC16(const QByteArray &data) {
    return CRC16::calculate(data);
}

void MainWindow::processReceivedData(const QByteArray &data) {
     QString dataString = QString::fromUtf8(data.toHex(' '));
     ui->serialMessageEdit->insertPlainText(u8"接收：" + dataString + "\r\n");

    // Verify packet structure
    if (data.size() < 18 || data.at(0) != 0x55) {
        ui->serialMessageEdit->insertPlainText(u8"数据接收错误！\r\n");
        return;
    }

    QByteArray receivedData = data.mid(1, data.size() - 4);
    uint16_t receivedCrc = *reinterpret_cast<const uint16_t*>(data.constData() + data.size() - 3);
    uint16_t calculatedCrc = calculateCRC16(receivedData);

    if (receivedCrc != calculatedCrc) {
        // Handle CRC error
        ui->serialMessageEdit->insertPlainText(u8"CRC校验出错！\r\n");
        return;
    }

    // 提取前六位数据（温度等信息）
    // QByteArray infoData = receivedData.left(6);

    // 提取七到十位数据（T轴电机角度）
    QByteArray tAxisData = receivedData.mid(6, 4);
    uint32_t tAxisAngle = *reinterpret_cast<const uint32_t*>(tAxisData.constData());
    double tAngle = static_cast<double>(tAxisAngle) / 100.0;
    double tRotation = tAngle - tOriginAngle;
    ui->tGaugePanelWidget->setValue(tRotation);

    // 提取十一到十四位数据（R轴电机角度）
    QByteArray rAxisData = receivedData.mid(10, 4);
    uint32_t rAxisAngle = *reinterpret_cast<const uint32_t*>(rAxisData.constData());
    double rAngle = static_cast<double>(rAxisAngle) / 100.0;
    double rRotation = rAngle - rOriginAngle;
    ui->rGaugePanelWidget->setValue(rRotation);
}

void MainWindow::on_rAxisForwardButton_clicked()
{
    if (m_serial->isOpen())
    {
        QByteArray packet = createPacket(rForwardData);
        m_serial->write(packet);
    }
}

void MainWindow::on_rAxisBackwardButton_clicked()
{
    if (m_serial->isOpen())
    {
        QByteArray packet = createPacket(rBackwardData);
        m_serial->write(packet);
    }
}

void MainWindow::on_tAxisForwardButton_clicked()
{
    if (m_serial->isOpen())
    {
        QByteArray packet = createPacket(tForwardData);
        m_serial->write(packet);
    }
}

void MainWindow::on_tAxisBackwardButton_clicked()
{
    if (m_serial->isOpen())
    {
        QByteArray packet = createPacket(tBackwardData);
        m_serial->write(packet);
    }
}

void MainWindow::onSpeedChanged()
{
    if (ui->lowSpeedRadioButton->isChecked())
    {
        rForwardData = QByteArray::fromHex("000000000000000100000000");
        rBackwardData = QByteArray::fromHex("00000000ffffffff00000000");
        tForwardData = QByteArray::fromHex("000000010000000000000000");
        tBackwardData = QByteArray::fromHex("ffffffff0000000000000000");
    }
    else if (ui->mediumSpeedRadioButton->isChecked())
    {
        rForwardData = QByteArray::fromHex("000000000000006400000000");
        rBackwardData = QByteArray::fromHex("00000000ffffff9c00000000");
        tForwardData = QByteArray::fromHex("000000640000000000000000");
        tBackwardData = QByteArray::fromHex("ffffff9c0000000000000000");
    }
    else if (ui->highSpeedRadioButton->isChecked())
    {
        rForwardData = QByteArray::fromHex("00000000000003e800000000");  // ff 00 00 00
        rBackwardData = QByteArray::fromHex("00000000fffffc1800000000"); // 9c 00 00 00
        tForwardData = QByteArray::fromHex("000003e80000000000000000");
        tBackwardData = QByteArray::fromHex("fffffc180000000000000000");  //fc 18 00 00
    }
}


