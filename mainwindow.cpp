#include <QMessageBox>
#include <QTimer>
#include <QFileDialog>
#include <QDebug>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "crc16.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isRecording(false)
    , m_hcam(nullptr)
    , m_timer(new QTimer(this))
    , m_serialTimer(new QTimer(this))
    , m_imgWidth(5440), m_imgHeight(3648), m_pData(nullptr)
    , m_res(0), m_temp(NNCAM_TEMP_DEF), m_tint(NNCAM_TINT_DEF)
    , m_red(0), m_green(0), m_blue(0), m_count(0)
    , m_pixmapItem(nullptr), m_aeItem(nullptr), m_awbItem(nullptr), m_abbItem(nullptr)
    , m_serial(new QSerialPort(this))
{
    ui->setupUi(this);
    // setWindowFlags(Qt::FramelessWindowHint);

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

    // 初始化相机预览窗口
    m_scene = new MyGraphicsScene(this);
    m_imageView = new QGraphicsView(m_scene, this);
    ui->imageViewLayout->addWidget(m_imageView);

    m_pixmapItem = new QGraphicsPixmapItem();
    m_scene->addItem(m_pixmapItem);

    connect(ui->lineMeasureButton, &QPushButton::clicked, m_scene, &MyGraphicsScene::startDrawingLine);
    connect(ui->lineDeleteButton, &QPushButton::clicked, m_scene, &MyGraphicsScene::removeSelectedLine);
    connect(m_scene, &MyGraphicsScene::addLineInfo, this, &MainWindow::addLineWidgets);

    // 设置默认自动曝光目标
    {
        const QSignalBlocker blocker(ui->exposureTargetSlider);
        ui->exposureTargetNumLabel->setText(QString::number(NNCAM_AETARGET_DEF));
        ui->exposureTargetSlider->setRange(NNCAM_AETARGET_MIN, NNCAM_AETARGET_MAX);
        ui->exposureTargetSlider->setValue(NNCAM_AETARGET_DEF);
    }

    // 设置默认白平衡参数
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

    // 设置默认黑平衡参数
    {
        const QSignalBlocker blocker(ui->redSlider);
        ui->redNumLabel->setText(QString::number(0));
        ui->redSlider->setRange(0, 255);
        ui->redSlider->setValue(0);
    }
    {
        const QSignalBlocker blocker(ui->greenSlider);
        ui->greenNumLabel->setText(QString::number(0));
        ui->greenSlider->setRange(0, 255);
        ui->greenSlider->setValue(0);
    }
    {
        const QSignalBlocker blocker(ui->blueSlider);
        ui->blueNumLabel->setText(QString::number(0));
        ui->blueSlider->setRange(0, 255);
        ui->blueSlider->setValue(0);
    }

    // 设置默认颜色调整参数
    {
        const QSignalBlocker blocker(ui->hueSlider);
        ui->hueNumLabel->setText(QString::number(NNCAM_HUE_DEF));
        ui->hueSlider->setRange(NNCAM_HUE_MIN, NNCAM_HUE_MAX);
        ui->hueSlider->setValue(NNCAM_HUE_DEF);
    }
    {
        const QSignalBlocker blocker(ui->saturationSlider);
        ui->saturationNumLabel->setText(QString::number(NNCAM_SATURATION_DEF));
        ui->saturationSlider->setRange(NNCAM_SATURATION_MIN, NNCAM_SATURATION_MAX);
        ui->saturationSlider->setValue(NNCAM_SATURATION_DEF);
    }
    {
        const QSignalBlocker blocker(ui->brightnessSlider);
        ui->brightnessNumLabel->setText(QString::number(NNCAM_BRIGHTNESS_DEF));
        ui->brightnessSlider->setRange(NNCAM_BRIGHTNESS_MIN, NNCAM_BRIGHTNESS_MAX);
        ui->brightnessSlider->setValue(NNCAM_BRIGHTNESS_DEF);
    }
    {
        const QSignalBlocker blocker(ui->contrastSlider);
        ui->contrastNumLabel->setText(QString::number(NNCAM_CONTRAST_DEF));
        ui->contrastSlider->setRange(NNCAM_CONTRAST_MIN, NNCAM_CONTRAST_MAX);
        ui->contrastSlider->setValue(NNCAM_CONTRAST_DEF);
    }
    {
        const QSignalBlocker blocker(ui->gammaSlider);
        ui->gammaNumLabel->setText(QString::number(NNCAM_GAMMA_DEF));
        ui->gammaSlider->setRange(NNCAM_GAMMA_MIN, NNCAM_GAMMA_MAX);
        ui->gammaSlider->setValue(NNCAM_GAMMA_DEF);
    }

    // 定时器更新帧率显示
    connect(m_timer, &QTimer::timeout, this, [this]()
    {
        unsigned nFrame = 0, nTime = 0, nTotalFrame = 0;
        if (m_hcam && SUCCEEDED(Nncam_get_FrameRate(m_hcam, &nFrame, &nTime, &nTotalFrame)) && (nTime > 0))
            ui->lblLabel->setText(QString::asprintf("%u, fps = %.1f", nTotalFrame, nFrame * 1000.0 / nTime));
    });

    // 连接tab关闭信号和槽
    ui->tabWidget->setTabsClosable(true);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(ui->lowSpeedRadioButton, &QRadioButton::clicked, this, &MainWindow::onSpeedChanged);
    connect(ui->mediumSpeedRadioButton, &QRadioButton::clicked, this, &MainWindow::onSpeedChanged);
    connect(ui->highSpeedRadioButton, &QRadioButton::clicked, this, &MainWindow::onSpeedChanged);

    // 设置档位滑动条
    {
        const QSignalBlocker blocker(ui->smallShiftSlider);
        ui->smallShiftSlider->setRange(0, 478);
    }

    // 设置量程单位
    {
        const QSignalBlocker blocker(ui->unitComboBox);
        ui->unitComboBox->clear();
        ui->unitComboBox->addItem("像素");
        ui->unitComboBox->addItem("微米");
        ui->unitComboBox->setCurrentIndex(0);
    }

    // 串口连接断开监测
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::handleSerialError);

    // 默认串口发送数据
    sendDataPacket = createPacket(defaultData);
    defaultDataPacket = createPacket(defaultData);

    // 串口x, y, z轴定时器
    // m_serialTimer->setInterval(33);
    connect(m_serialTimer, &QTimer::timeout, this, &MainWindow::sendData);

    // x轴按钮
    connect(ui->xAxisForwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(xForwardData);
    });
    connect(ui->xAxisForwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });
    connect(ui->xAxisBackwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(xBackwardData);
    });
    connect(ui->xAxisBackwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });

    // y轴按钮
    connect(ui->yAxisForwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(yForwardData);
    });
    connect(ui->yAxisForwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });
    connect(ui->yAxisBackwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(yBackwardData);
    });
    connect(ui->yAxisBackwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });

    // z轴按钮
    connect(ui->zAxisForwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(zForwardData);
    });
    connect(ui->zAxisForwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });
    connect(ui->zAxisBackwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(zBackwardData);
    });
    connect(ui->zAxisBackwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });

    // r轴按钮
    connect(ui->rAxisForwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(rForwardData);
    });
    connect(ui->rAxisForwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });
    connect(ui->rAxisBackwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(rBackwardData);
    });
    connect(ui->rAxisBackwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });

    // t轴按钮
    connect(ui->tAxisForwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(tForwardData);
    });
    connect(ui->tAxisForwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });
    connect(ui->tAxisBackwardButton, &QPushButton::pressed, this, [this]() {
        sendDataPacket = createPacket(tBackwardData);
    });
    connect(ui->tAxisBackwardButton, &QPushButton::released, this, [this]() {
        sendDataPacket = defaultDataPacket;
    });

    this->showMaximized();
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

    if (m_serial)
    {
        closeSerial();

        delete m_serialTimer;
        m_serialTimer = nullptr;
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    QRect layoutRect = ui->imageViewLayout->geometry();
    m_previewWidth = layoutRect.width() - 30;
    float ratio = float(m_previewWidth) / m_imgWidth;
    m_previewHeight = int(m_imgHeight * ratio);
    m_scene->setSceneRect(0, 0, m_previewWidth, m_previewHeight);
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

                QPixmap pixmap = QPixmap::fromImage(image.scaled(newTab->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                imageLabel->setScaledContents(true);
                imageLabel->setPixmap(pixmap);

                // 将新创建的QImage存储到vetor中，并添加标签页到tabWidget
                ui->tabWidget->addTab(newTab, QString("image_") + QString::number(++m_count));
                imageVector.append(image);
            }
        }
        else
        {
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
            QMessageBox::warning(this, "Warning", u8"请先打开相机。");
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
        }
    }
    else
    {
        // 停止录制
        if (m_isRecording)
        {
            m_isRecording = false;
            m_videoWriter.release();
            ui->videoButton->setText("录像");
        }
    }
}

void MainWindow::on_previewComboBox_currentIndexChanged(int index)
{
    if (m_isRecording)
    {
        QMessageBox::warning(this, "Warning", u8"请先停止录像。");
        return;
    }

    if (m_hcam)
        Nncam_Stop(m_hcam);

    m_res = index;
    m_imgWidth = m_cur.model->res[index].width;
    m_imgHeight = m_cur.model->res[index].height;

    if (m_hcam)
    {
        Nncam_put_eSize(m_hcam, static_cast<unsigned>(m_res));
        Nncam_get_PixelSize(m_hcam, static_cast<unsigned>(m_res), &m_xpixsz, &m_ypixsz);

        startCamera();
    }
}

void MainWindow::on_autoExposureCheckBox_stateChanged(int state)
{
    ui->exposureTargetSlider->setEnabled(state);
    ui->exposureTimeSlider->setEnabled(!state);
    ui->gainSlider->setEnabled(!state);

    if (state)
    {
        if (m_aeItem == nullptr)
        {
            if (m_hcam)
            {
                if (SUCCEEDED(Nncam_get_AEAuxRect(m_hcam, &m_aeRect)))
                {
                    QString orectString = QString("oRect(left: %1, top: %2, right: %3, bottom: %4)")
                            .arg(m_aeRect.left)
                            .arg(m_aeRect.top)
                            .arg(m_aeRect.right)
                            .arg(m_aeRect.bottom);

                    qDebug() << orectString; // 输出格式化后的字符串

                    float left = static_cast<float>(m_aeRect.left) / m_imgWidth;
                    float top = static_cast<float>(m_aeRect.top) / m_imgHeight;
                    float right = static_cast<float>(m_aeRect.right) / m_imgWidth;
                    float bottom = static_cast<float>(m_aeRect.bottom) / m_imgHeight;

                    m_aeItem = new RectItem();
                    qDebug() << m_previewWidth << m_previewHeight;
                    qDebug() << m_imgWidth << m_imgHeight;
                    m_aeItem->initRect(left, top, right, bottom, m_previewWidth, m_previewHeight);
                    m_aeItem->setColor(Qt::red);
                    m_scene->addItem(m_aeItem);
                    connect(m_aeItem, &RectItem::rectChanged, this, &MainWindow::onAERectChanged);

                    onAERectChanged(left, top, right, bottom);
                }
            }
        }
        else
        {
            if (m_hcam)
            {
                if (SUCCEEDED(Nncam_put_AutoExpoEnable(m_hcam, 1)))
                {}
                else
                {
                    QMessageBox::warning(this, "Warning", u8"自动曝光设置失败。");
                }
            }
        }

        unsigned short target = 0;
        if (m_hcam)
        {
            if (SUCCEEDED(Nncam_get_AutoExpoTarget(m_hcam, &target)))
            {
                {
                    const QSignalBlocker blocker(ui->exposureTargetSlider);
                    ui->exposureTargetSlider->setValue(int(target));
                    ui->exposureTargetNumLabel->setText(QString::number(target));
                }
            }
        }

        m_aeItem->setVisible(1);
    }
    else
    {
        if (m_hcam)
        {
            unsigned time = 0;
            if (SUCCEEDED(Nncam_get_ExpoTime(m_hcam, &time)))
            {
                {
                    const QSignalBlocker blocker(ui->exposureTimeSlider);
                    ui->exposureTimeSlider->setValue(int(time));
                    ui->exposureTimeNumLabel->setText(QString::number(time));
                }
            }
        }

        if (m_hcam)
        {
            unsigned short gain = 0;
            if (SUCCEEDED(Nncam_get_ExpoAGain(m_hcam, &gain)))
            {
                {
                    const QSignalBlocker blocker(ui->gainSlider);
                    ui->gainSlider->setValue(int(gain));
                    ui->gainNumLabel->setText(QString::number(gain));
                }
            }
        }

        m_aeItem->setVisible(0);
    }
}

void MainWindow::on_exposureTargetSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (ui->autoExposureCheckBox->isChecked())
        {
            if (SUCCEEDED(Nncam_put_AutoExpoTarget(m_hcam, static_cast<unsigned short>(value))))
            {
                m_target = value;
                ui->exposureTargetNumLabel->setText(QString::number(value));
            }
            else
            {
                {
                    const QSignalBlocker blocker(ui->exposureTargetSlider);
                    ui->exposureTargetSlider->setValue(m_target);
                }
                QMessageBox::warning(this, "Warning", u8"调整自动曝光目标失败。"); 
            }
        }
    }
}

void MainWindow::on_exposureTimeSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoExposureCheckBox->isChecked())
        {
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
}

void MainWindow::on_gainSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoExposureCheckBox->isChecked())
        {
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
}

void MainWindow::on_autoAwbCheckBox_stateChanged(int state)
{
    ui->temperatureSlider->setEnabled(!state);
    ui->tintSlider->setEnabled(!state);

    if (state)
    {
        if (m_awbItem == nullptr)
        {
            if (m_hcam)
            {
                if (SUCCEEDED(Nncam_get_AWBAuxRect(m_hcam, &m_awbRect)))
                {
                    QString orectString = QString("oRect(left: %1, top: %2, right: %3, bottom: %4)")
                            .arg(m_awbRect.left)
                            .arg(m_awbRect.top)
                            .arg(m_awbRect.right)
                            .arg(m_awbRect.bottom);

                    qDebug() << orectString; // 输出格式化后的字符串

                    float left = static_cast<float>(m_awbRect.left) / m_imgWidth;
                    float top = static_cast<float>(m_awbRect.top) / m_imgHeight;
                    float right = static_cast<float>(m_awbRect.right) / m_imgWidth;
                    float bottom = static_cast<float>(m_awbRect.bottom) / m_imgHeight;

                    m_awbItem = new RectItem();
                    m_awbItem->initRect(left, top, right, bottom, m_previewWidth, m_previewHeight);
                    m_awbItem->setColor(Qt::blue);
                    m_scene->addItem(m_awbItem);
                    connect(m_awbItem, &RectItem::rectChanged, this, &MainWindow::onAWBRectChanged);

                    onAWBRectChanged(left, top, right, bottom);
                }
            }
        }
        else
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
        m_awbItem->setVisible(1);
    }
    else
    {
        if (m_hcam)
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

        m_awbItem->setVisible(0);
    }
}

void MainWindow::on_temperatureSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoAwbCheckBox->isChecked())
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
}

void MainWindow::on_tintSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoAwbCheckBox->isChecked())
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
                    ui->tintSlider->setValue(m_tint);
                }
                QMessageBox::warning(this, "Warning", u8"调整Tint失败。");          
            }
        }
    }
}

void MainWindow::on_defaultAwbButton_clicked()
{
    ui->temperatureSlider->setValue(NNCAM_TEMP_DEF);
    ui->tintSlider->setValue(NNCAM_TINT_DEF);
}

void MainWindow::on_autoAbbCheckBox_stateChanged(int state)
{
    ui->redSlider->setEnabled(!state);
    ui->greenSlider->setEnabled(!state);
    ui->blueSlider->setEnabled(!state);

    if (state)
    {
        if (m_abbItem == nullptr)
        {
            if (m_hcam)
            {
                if (SUCCEEDED(Nncam_get_ABBAuxRect(m_hcam, &m_abbRect)))
                {
                    QString orectString = QString("oRect(left: %1, top: %2, right: %3, bottom: %4)")
                            .arg(m_abbRect.left)
                            .arg(m_abbRect.top)
                            .arg(m_abbRect.right)
                            .arg(m_abbRect.bottom);

                    qDebug() << orectString; // 输出格式化后的字符串

                    float left = static_cast<float>(m_abbRect.left) / m_imgWidth;
                    float top = static_cast<float>(m_abbRect.top) / m_imgHeight;
                    float right = static_cast<float>(m_abbRect.right) / m_imgWidth;
                    float bottom = static_cast<float>(m_abbRect.bottom) / m_imgHeight;

                    m_abbItem = new RectItem();
                    m_abbItem->initRect(left, top, right, bottom, m_previewWidth, m_previewHeight);
                    m_abbItem->setColor(Qt::green);
                    m_scene->addItem(m_abbItem);
                    connect(m_abbItem, &RectItem::rectChanged, this, &MainWindow::onABBRectChanged);

                    onABBRectChanged(left, top, right, bottom);
                }
            }
        }
        else
        {
            if (m_hcam)
            {
                if (SUCCEEDED(Nncam_AbbOnce(m_hcam, nullptr, nullptr)))
                {
                }
                else
                {
                    QMessageBox::warning(this, "Warning", u8"自动黑平衡调整失败。"); 
                }
            }
        }
        m_abbItem->setVisible(1);
    }
    else
    {
        if (m_hcam)
        {
            if (SUCCEEDED(Nncam_get_BlackBalance(m_hcam, m_aSub)))
            {
                m_red = m_aSub[0];
                m_green = m_aSub[1];
                m_blue = m_aSub[2];
                {
                    const QSignalBlocker blocker(ui->redSlider);
                    ui->redSlider->setValue(m_red);
                    ui->redNumLabel->setText(QString::number(m_red));
                }
                {
                    const QSignalBlocker blocker(ui->greenSlider);
                    ui->greenSlider->setValue(m_green);
                    ui->greenNumLabel->setText(QString::number(m_green));
                }
                {
                    const QSignalBlocker blocker(ui->blueSlider);
                    ui->blueSlider->setValue(m_blue);
                    ui->blueNumLabel->setText(QString::number(m_blue));
                }
            }
        }
        m_abbItem->setVisible(0);
    }
}

void MainWindow::on_redSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoAbbCheckBox->isChecked())
        {
            m_aSub[0] = value;
            if (SUCCEEDED(Nncam_put_BlackBalance(m_hcam, m_aSub)))
            {
                m_red = value;
                ui->redNumLabel->setText(QString::number(value));
            }
            else
            {
                {
                    const QSignalBlocker blocker(ui->redSlider);
                    ui->redSlider->setValue(m_red);
                }
                QMessageBox::warning(this, "Warning", u8"黑平衡红色偏移调整失败。"); 
            }
        }
    }
}

void MainWindow::on_greenSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoAbbCheckBox->isChecked())
        {
            m_aSub[1] = value;
            if (SUCCEEDED(Nncam_put_BlackBalance(m_hcam, m_aSub)))
            {
                m_green = value;
                ui->greenNumLabel->setText(QString::number(value));
            }
            else
            {
                {
                    const QSignalBlocker blocker(ui->greenSlider);
                    ui->greenSlider->setValue(m_green);
                }
                QMessageBox::warning(this, "Warning", u8"黑平衡绿色偏移调整失败。"); 
            }
        }
    }
}

void MainWindow::on_blueSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        if (!ui->autoAbbCheckBox->isChecked())
        {
            m_aSub[2] = value;
            if (SUCCEEDED(Nncam_put_BlackBalance(m_hcam, m_aSub)))
            {
                m_blue = value;
                ui->blueNumLabel->setText(QString::number(value));
            }
            else
            {
                {
                    const QSignalBlocker blocker(ui->blueSlider);
                    ui->blueSlider->setValue(m_blue);
                }
                QMessageBox::warning(this, "Warning", u8"黑平衡蓝色偏移调整失败。"); 
            }
        }
    }
}

void MainWindow::on_defaultAbbButton_clicked()
{
    ui->redSlider->setValue(0);
    ui->greenSlider->setValue(0);
    ui->blueSlider->setValue(0);
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
    ui->hueSlider->setValue(0);
    ui->saturationSlider->setValue(128);
    ui->brightnessSlider->setValue(0);
    ui->contrastSlider->setValue(0);
    ui->gammaSlider->setValue(100);
}

void MainWindow::onAERectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio)
{
    m_aeRect.left = static_cast<int>(leftRatio * m_imgWidth);
    m_aeRect.top = static_cast<int>(topRatio * m_imgHeight);
    m_aeRect.right = static_cast<int>(rightRatio * m_imgWidth);
    m_aeRect.bottom = static_cast<int>(bottomRatio * m_imgHeight);

    QString rectString = QString("Rect(left: %1, top: %2, right: %3, bottom: %4)")
                             .arg(m_aeRect.left)
                             .arg(m_aeRect.top)
                             .arg(m_aeRect.right)
                             .arg(m_aeRect.bottom);

    qDebug() << rectString; // 输出格式化后的字符串
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_AEAuxRect(m_hcam, &m_aeRect)))
        {
            if (SUCCEEDED(Nncam_put_AutoExpoEnable(m_hcam, 1)))
            {
                qDebug() << "auto ae设置成功\n";
            }
            else
            {
                qDebug() << "auto ae设置失败\n";
                QMessageBox::warning(this, "Warning", u8"自动曝光设置失败。");
            }
        }
        else
        {
            qDebug() << "ae rect 设置失败\n";
            QMessageBox::warning(this, "Warning", u8"自动曝光设置失败。");
        }
    }
}

void MainWindow::onAWBRectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio)
{
    m_awbRect.left = static_cast<int>(leftRatio * m_imgWidth);
    m_awbRect.top = static_cast<int>(topRatio * m_imgHeight);
    m_awbRect.right = static_cast<int>(rightRatio * m_imgWidth);
    m_awbRect.bottom = static_cast<int>(bottomRatio * m_imgHeight);

    QString rectString = QString("Rect(left: %1, top: %2, right: %3, bottom: %4)")
                             .arg(m_awbRect.left)
                             .arg(m_awbRect.top)
                             .arg(m_awbRect.right)
                             .arg(m_awbRect.bottom);

    qDebug() << rectString; // 输出格式化后的字符串
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_AWBAuxRect(m_hcam, &m_awbRect)))
        {
            if (SUCCEEDED(Nncam_AwbOnce(m_hcam, nullptr, nullptr)))
            {}
            else
            {
                QMessageBox::warning(this, "Warning", u8"自动白平衡调整失败。"); 
            }
        }
        else
        {
            qDebug() << "awb rect 设置失败\n";
            QMessageBox::warning(this, "Warning", u8"自动白平衡调整失败。");
        }
    }
}

void MainWindow::onABBRectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio)
{
    m_abbRect.left = static_cast<int>(leftRatio * m_imgWidth);
    m_abbRect.top = static_cast<int>(topRatio * m_imgHeight);
    m_abbRect.right = static_cast<int>(rightRatio * m_imgWidth);
    m_abbRect.bottom = static_cast<int>(bottomRatio * m_imgHeight);

    QString rectString = QString("Rect(left: %1, top: %2, right: %3, bottom: %4)")
                             .arg(m_abbRect.left)
                             .arg(m_abbRect.top)
                             .arg(m_abbRect.right)
                             .arg(m_abbRect.bottom);

    qDebug() << rectString; // 输出格式化后的字符串
    if (m_hcam)
    {
        if (SUCCEEDED(Nncam_put_ABBAuxRect(m_hcam, &m_abbRect)))
        {
            if (SUCCEEDED(Nncam_AbbOnce(m_hcam, nullptr, nullptr)))
            {}
            else
            {
                QMessageBox::warning(this, "Warning", u8"自动黑平衡调整失败。"); 
            }
        }
        else
        {
            qDebug() << "abb rect 设置失败\n";
            QMessageBox::warning(this, "Warning", u8"自动黑平衡调整失败。");
        }
    }
}

void MainWindow::on_unitComboBox_currentIndexChanged(int index)
{
    m_measureFlag = index;
}

void MainWindow::addLineWidgets(QGraphicsLineItem* lineItem, QPointF startPoint, QPointF endPoint) {

    // 获取起始点和结束点的 x 和 y 坐标，并转换为 float
    float startScreenX = static_cast<float>(startPoint.x());
    float startScreenY = static_cast<float>(startPoint.y());
    float endScreenX = static_cast<float>(endPoint.x());
    float endScreenY = static_cast<float>(endPoint.y());
    // float screen2pixelRatio = static_cast<float>(m_previewWidth) / m_imgWidth;

    float startPixelX = startScreenX * m_imgWidth / m_previewWidth;
    float startPixelY = startScreenY * m_imgHeight / m_previewHeight;
    float endPixelX = endScreenX * m_imgWidth / m_previewWidth;
    float endPixelY = endScreenY * m_imgHeight / m_previewHeight;
    
    if (m_measureFlag == 0)
    {
        float deltaX = endPixelX - startPixelX;
        float deltaY = endPixelY - startPixelY;
        m_distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }
    else{
        float startRealX = startPixelX * m_xpixsz;
        float startRealY = startPixelY * m_ypixsz;
        float endRealX = endPixelX * m_xpixsz;
        float endRealY = endPixelY * m_ypixsz;

        float deltaX = endRealX - startRealX;
        float deltaY = endRealY - startRealY;
        m_distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }

    QLabel* label = new QLabel(QString("长度: %2").arg(m_distance), this);
    QPushButton* deleteButton = new QPushButton("删除", this);

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addWidget(label);
    hLayout->addWidget(deleteButton);
    QWidget* layoutWidget = new QWidget(this);
    layoutWidget->setLayout(hLayout);

    // Store the QLabel and QPushButton
    labels[lineItem] = label;
    deleteButtons[lineItem] = deleteButton;
    layoutWidgets[lineItem] = layoutWidget;
    ui->measureResultLayout->addWidget(layoutWidget);

    // Connect delete button to the line removal slot
    connect(deleteButton, &QPushButton::clicked, [this, lineItem]() {
        m_scene->removeLine(lineItem);
        removeLineWidgets(lineItem);
    });
}

void MainWindow::removeLineWidgets(QGraphicsLineItem* lineItem) {
    if (labels.contains(lineItem)) {
        QLabel* label = labels.take(lineItem);
        QPushButton* deleteButton = deleteButtons.take(lineItem);
        QWidget* layoutWidget = layoutWidgets.take(lineItem);

        delete label;
        delete deleteButton;
        delete layoutWidget;
    }
}

void MainWindow::openCamera()
{
    // 打开摄像头
    m_hcam = Nncam_Open(m_cur.id);
    if (m_hcam)
    {
        // 设置帧速率级别
        // unsigned short maxSpeed = Nncam_get_MaxSpeed(m_hcam);
        // Nncam_put_Speed(m_hcam, maxSpeed);

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
            qDebug() << uimax << uimin << uidef;  // 3600000000 100 2000  // 5s 0.1ms
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

        // 处理画面比例
        QRect layoutRect = ui->imageViewLayout->geometry();
        m_previewWidth = layoutRect.width() - 30;
        float ratio = float(m_previewWidth) / m_imgWidth;
        m_previewHeight = int(m_imgHeight * ratio);
        m_scene->setSceneRect(0, 0, m_previewWidth, m_previewHeight);

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
            "您有未保存的图像，是否保存？",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes)
        {
            // 用户选择保存图像
            for (int i = 0; i < imageVector.size(); ++i)
            {
                closeTab(i+1);
            }
        }
        else if (reply == QMessageBox::Cancel)
        {
            // 用户选择取消，不关闭相机
            return 0;
        }
    }

    // 如果没有图像保存或者用户选择不保存，继续关闭相机并进行内存回收

    // 移除所有标签页
    while (ui->tabWidget->count() > 1)
    {
        QWidget *widget = ui->tabWidget->widget(1);
        ui->tabWidget->removeTab(1);
        delete widget;
    }
    // 清空图像向量
    imageVector.clear();

    // 停止录像
    if (m_isRecording)
    {
        m_isRecording = false;
        m_videoWriter.release();
        ui->videoButton->setText("录像");
    }

    // 删除预览线程
    if (m_cameraThread)
    {
        delete m_cameraThread;
        m_cameraThread = nullptr;
    }

    // 关闭相机
    if (m_hcam)
    {
        Nncam_Close(m_hcam);
        m_hcam = nullptr;
    }

    delete[] m_pData;
    m_pData = nullptr;
    delete m_imageView;
    m_imageView = nullptr;
    delete m_scene;
    m_scene = nullptr;
    delete m_pixmapItem;
    m_pixmapItem = nullptr;

    // 停止计时
    if (m_timer->isActive()) {
        m_timer->stop();
    }

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

    ui->autoAwbCheckBox->setEnabled(false);
    ui->defaultAwbButton->setEnabled(false);
    ui->temperatureSlider->setEnabled(false);
    ui->tintSlider->setEnabled(false);

    ui->autoAbbCheckBox->setEnabled(false);
    ui->defaultAbbButton->setEnabled(false);
    ui->redSlider->setEnabled(false);
    ui->greenSlider->setEnabled(false);
    ui->blueSlider->setEnabled(false);

    ui->hueSlider->setEnabled(false);
    ui->saturationSlider->setEnabled(false);
    ui->brightnessSlider->setEnabled(false);
    ui->contrastSlider->setEnabled(false);
    ui->gammaSlider->setEnabled(false);
    ui->defaultColorButton->setEnabled(false);

    ui->lineMeasureButton->setEnabled(false);
    ui->lineDeleteButton->setEnabled(false);
    ui->unitComboBox->setEnabled(false);

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
    QImage newImage = image.scaled(m_previewWidth, m_previewHeight, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_pixmapItem->setPixmap(QPixmap::fromImage(newImage));

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

        QPixmap pixmap = QPixmap::fromImage(image);
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

        // 使能曝光功能
        ui->autoExposureCheckBox->setEnabled(true);
        int bAuto = 0;
        Nncam_get_AutoExpoEnable(m_hcam, &bAuto);
        ui->exposureTargetSlider->setEnabled(1 == bAuto);
        ui->exposureTimeSlider->setEnabled(!(1 == bAuto));
        ui->gainSlider->setEnabled(!(1 == bAuto));
        {
            const QSignalBlocker blocker(ui->autoExposureCheckBox);
            ui->autoExposureCheckBox->setChecked(1 == bAuto);
        }

        // 使能白平衡功能
        ui->autoAwbCheckBox->setEnabled(true);
        ui->defaultAwbButton->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->temperatureSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->tintSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        {
            const QSignalBlocker blocker(ui->autoAwbCheckBox);
            ui->autoAwbCheckBox->setChecked(false);
        }

        // 使能黑平衡功能
        ui->autoAbbCheckBox->setEnabled(true);
        ui->defaultAbbButton->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->redSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->greenSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        ui->blueSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        {
            const QSignalBlocker blocker(ui->autoAbbCheckBox);
            ui->autoAbbCheckBox->setChecked(false);
        }

        // 使能颜色调整功能
        ui->hueSlider->setEnabled(true);
        ui->saturationSlider->setEnabled(true);
        ui->brightnessSlider->setEnabled(true);
        ui->contrastSlider->setEnabled(true);
        ui->gammaSlider->setEnabled(true);
        ui->defaultColorButton->setEnabled(true);

        // 使能测量工具
        ui->lineMeasureButton->setEnabled(true);
        ui->lineDeleteButton->setEnabled(true);
        ui->unitComboBox->setEnabled(true);

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
        return;

    else if (index > 0 && index < ui->tabWidget->count() && index <= imageVector.size())
    {
        QImage image = imageVector.at(index-1);

        QString filename = QString::asprintf("image_%u.jpg", index-1);
        QString path = QFileDialog::getSaveFileName(this, "Save Image", filename, "JPEG Files (*.jpg)");

        // 检查用户是否取消了对话框
        if (!path.isEmpty())
        {
            // 用户选择了保存路径，保存图像
            image.save(path);

            // 从vector中移除对应的QImage
            imageVector.removeAt(index-1);
            QWidget *widget = ui->tabWidget->widget(index);
            ui->tabWidget->removeTab(index);
            delete widget;
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
        ui->xAxisForwardButton->setEnabled(true);
        ui->xAxisBackwardButton->setEnabled(true);
        ui->yAxisForwardButton->setEnabled(true);
        ui->yAxisBackwardButton->setEnabled(true);
        ui->zAxisForwardButton->setEnabled(true);
        ui->zAxisBackwardButton->setEnabled(true);
        ui->bigShiftButton->setEnabled(true);
        ui->smallShiftSlider->setEnabled(true);

        ui->serialMessageEdit->insertPlainText(u8"串口连接成功！\r\n");

        m_serialTimer->start(23);
    }
    else
    {
        closeSerial();
    }
}

void MainWindow::closeSerial()
{
        if (m_serialTimer->isActive()) {
            m_serialTimer->stop();
        }

        if (m_serial->isOpen())  //原先串口打开，则关闭串口
            m_serial->close();

        //释放串口
        delete[] m_serial;
        m_serial = nullptr;

        //恢复使能
        ui->portBox->setEnabled(true);
        ui->openSerialButton->setText(u8"打开串口");
        ui->openSerialButton->setStyleSheet("");
        ui->highSpeedRadioButton->setEnabled(false);
        ui->mediumSpeedRadioButton->setEnabled(false);
        ui->lowSpeedRadioButton->setEnabled(false);
        ui->rAxisForwardButton->setEnabled(false);
        ui->rAxisBackwardButton->setEnabled(false);
        ui->tAxisForwardButton->setEnabled(false);
        ui->tAxisBackwardButton->setEnabled(false);
        ui->xAxisForwardButton->setEnabled(false);
        ui->xAxisBackwardButton->setEnabled(false);
        ui->yAxisForwardButton->setEnabled(false);
        ui->yAxisBackwardButton->setEnabled(false);
        ui->zAxisForwardButton->setEnabled(false);
        ui->zAxisBackwardButton->setEnabled(false);
        ui->bigShiftButton->setEnabled(false);
        ui->smallShiftSlider->setEnabled(false);
}

QByteArray MainWindow::createPacket(const QByteArray &data) {
    QByteArray packet;
    packet.append(0x55);  // Start byte
    packet.append(data);

    uint16_t crc = CRC16::calculate(data);
    packet.append(reinterpret_cast<const char*>(&crc), sizeof(crc));

    packet.append(0xAA);  // End byte
    return packet;
}

void MainWindow::readSerialData()
{
    QByteArray data = m_serial->readAll();
    processReceivedData(data);
}

void MainWindow::processReceivedData(const QByteArray &data) {
    QString dataString = QString::fromUtf8(data.toHex(' '));
    ui->serialMessageEdit->insertPlainText(u8"接收：" + dataString + "\r\n");
    qDebug() << "1";
    // Verify packet structure
    // if (data.size() < 19 || data.at(0) != 0x55) {
    //     ui->serialMessageEdit->insertPlainText(u8"数据接收错误！\r\n");
    //     return;
    // }
}

void MainWindow::on_bigShiftButton_clicked()
{
    if (m_serial->isOpen())
    {
        m_bigShiftFlag = 1;
        sendDataPacket = createPacket(bigShiftData);
    }
}

void MainWindow::on_smallShiftSlider_valueChanged(int value)
{
    if (m_serial->isOpen())
    {
        unsigned short fInitialValue = 0x01ff;
        unsigned short fNewValue = fInitialValue - value;
        fNewValue = fNewValue & 0xFFFF;
        yForwardData[8] = static_cast<char>(fNewValue >> 8); // 高字节
        yForwardData[9] = static_cast<char>(fNewValue & 0xFF); // 低字节
        xForwardData[10] = static_cast<char>(fNewValue >> 8); // 高字节
        xForwardData[11] = static_cast<char>(fNewValue & 0xFF); // 低字节
        zForwardData[12] = static_cast<char>(fNewValue >> 8); // 高字节
        zForwardData[13] = static_cast<char>(fNewValue & 0xFF); // 低字节

        unsigned short bInitialValue = 0x0201;
        unsigned short bNewValue = bInitialValue + value;
        bNewValue = bNewValue & 0xFFFF;
        yBackwardData[8] = static_cast<char>(bNewValue >> 8); // 高字节
        yBackwardData[9] = static_cast<char>(bNewValue & 0xFF); // 低字节
        xBackwardData[10] = static_cast<char>(bNewValue >> 8); // 高字节
        xBackwardData[11] = static_cast<char>(bNewValue & 0xFF); // 低字节
        zBackwardData[12] = static_cast<char>(bNewValue >> 8); // 高字节
        zBackwardData[13] = static_cast<char>(bNewValue & 0xFF); // 低字节
    }
}

void MainWindow::onSpeedChanged()
{
    if (ui->lowSpeedRadioButton->isChecked())
    {
        rForwardData = QByteArray::fromHex("000000000000000102000200020000");
        rBackwardData = QByteArray::fromHex("00000000ffffffff02000200020000");

        tForwardData = QByteArray::fromHex("000000010000000002000200020000");
        tBackwardData = QByteArray::fromHex("ffffffff0000000002000200020000");
    }
    else if (ui->mediumSpeedRadioButton->isChecked())
    {
        rForwardData = QByteArray::fromHex("000000000000006402000200020000");
        rBackwardData = QByteArray::fromHex("00000000ffffff9c02000200020000");

        tForwardData = QByteArray::fromHex("000000640000000002000200020000");
        tBackwardData = QByteArray::fromHex("ffffff9c0000000002000200020000");
    }
    else if (ui->highSpeedRadioButton->isChecked())
    {
        rForwardData = QByteArray::fromHex("00000000000003e802000200020000");
        rBackwardData = QByteArray::fromHex("00000000fffffc1802000200020000");

        tForwardData = QByteArray::fromHex("000003e80000000002000200020000");
        tBackwardData = QByteArray::fromHex("fffffc180000000002000200020000");
    }
}

void MainWindow::sendData()
{
    if (m_serial->isOpen())
    {
        if (m_bigShiftFlag > 0)
        {
            m_bigShiftFlag += 1;
        } 
        if (m_bigShiftFlag > 10)
        {
            m_bigShiftFlag = 0;
            sendDataPacket = defaultDataPacket;
        }
        qDebug() << sendDataPacket;
        m_serial->write(sendDataPacket);
    }
}

void MainWindow::handleSerialError(QSerialPort::SerialPortError error) {
    closeSerial();
    QMessageBox::warning(this, "Warning", u8"微位移串口连接出错！");
}