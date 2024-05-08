#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>
#include <windows.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pWmvRecord(nullptr)
    , m_hcam(nullptr)
    , m_timer(new QTimer(this))
    , m_imgWidth(0), m_imgHeight(0), m_pData(nullptr)
    , m_res(0), m_temp(NNCAM_TEMP_DEF), m_tint(NNCAM_TINT_DEF), m_count(0)
{
    ui->setupUi(this);
    updateMainStyle("myDark.qss");

    ui->temperatureNumLabel->setText(QString::number(NNCAM_TEMP_DEF));
    ui->tintNumLabel->setText(QString::number(NNCAM_TINT_DEF));
    ui->temperatureSlider->setRange(NNCAM_TEMP_MIN, NNCAM_TEMP_MAX);
    ui->temperatureSlider->setValue(NNCAM_TEMP_DEF);
    ui->tintSlider->setRange(NNCAM_TINT_MIN, NNCAM_TINT_MAX);
    ui->tintSlider->setValue(NNCAM_TINT_DEF);

    connect(this, &MainWindow::evtCallback, this, [this](unsigned nEvent)
    {
        if (m_hcam)
        {
            if (NNCAM_EVENT_IMAGE == nEvent)
                handleImageEvent();
            else if (NNCAM_EVENT_EXPOSURE == nEvent)
                handleExpoEvent();
            else if (NNCAM_EVENT_TEMPTINT == nEvent)
                handleTempTintEvent();
            else if (NNCAM_EVENT_STILLIMAGE == nEvent)
                handleStillImageEvent();
            else if (NNCAM_EVENT_ERROR == nEvent)
            {
                closeCamera();
                QMessageBox::warning(this, "Warning", "Generic error.");
            }
            else if (NNCAM_EVENT_DISCONNECTED == nEvent)
            {
                closeCamera();
                QMessageBox::warning(this, "Warning", "Camera disconnect.");
            }
        }
    });

    connect(m_timer, &QTimer::timeout, this, [this]()
    {
        unsigned nFrame = 0, nTime = 0, nTotalFrame = 0;
        if (m_hcam && SUCCEEDED(Nncam_get_FrameRate(m_hcam, &nFrame, &nTime, &nTotalFrame)) && (nTime > 0))
            ui->lblLabel->setText(QString::asprintf("%u, fps = %.1f", nTotalFrame, nFrame * 1000.0 / nTime));
    });

    // 连接tab关闭信号和槽
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::closeTab);
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

                QPixmap pixmap = QPixmap::fromImage(image);
                // QPixmap pixmap = QPixmap::fromImage(image).scaled(newTab->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                // imageLabel->setScaledContents(true);
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
        // 创建并打开文件对话框，让用户选择保存WMV文件的位置
        QFileDialog dlg(this, tr("Save WMV File"), "", tr("WMV Files (*.wmv)"));
        dlg.setFileMode(QFileDialog::AnyFile);
        dlg.setAcceptMode(QFileDialog::AcceptSave);

        if (dlg.exec() == QDialog::Accepted)
        {
            QString fileName = dlg.selectedFiles().first();
            if (!fileName.isEmpty())
            {
                // 停止之前的录制
                stopRecord();

                // 设置比特率
                DWORD dwBitrate = 4 * 1024 * 1024; // 比特率，可以根据需要修改

                // 创建WmvRecorder对象
                m_pWmvRecord = new CWmvRecord(static_cast<LONG>(m_imgWidth), static_cast<LONG>(m_imgHeight));
                if (m_pWmvRecord->StartRecord(fileName.toStdWString().c_str(), dwBitrate))
                {
                    // 启用和禁用相应的UI组件
                    ui->videoButton->setText("停止录像");
                }
                else
                {
                    QMessageBox::warning(this, tr("Record Error"), tr("Failed to start recording."));
                    delete m_pWmvRecord;
                    m_pWmvRecord = nullptr;
                }
            }
        }
    }
    else
    {
        // 停止录制
        stopRecord();

        // 启用和禁用相应的UI组件
        ui->videoButton->setText("录像");
    }
}

void MainWindow::on_whileBalanceButton_clicked()
{
    Nncam_AwbOnce(m_hcam, nullptr, nullptr);
}

void MainWindow::on_defaultValueButton_clicked()
{

}

void MainWindow::on_searchCameraButton_clicked()
{
    // 如果相机已打开，则关闭相机
    if (m_hcam)
    {
        closeCamera();
        // unsigned count = 0;
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
            QMessageBox::warning(this, "Warning", "No camera found.");
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
            ui->cameraComboBox->setCurrentIndex(0);
            m_cur = arr[0];
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

void MainWindow::initPreview()
{
    // 创建预览页面、视图和场景
    previewTab = new QWidget();
    previewView = new QGraphicsView();
    previewScene = new QGraphicsScene();
    pixmapItem = previewScene->addPixmap(framePixmap);

    // 设置previewView的场景
    previewView->setScene(previewScene);
    // 将previewView添加到previewTab的布局中
    QGridLayout *previewLayout = new QGridLayout(previewTab);
    previewLayout->addWidget(previewView);
    // 设置previewTab的布局
    previewTab->setLayout(previewLayout);
    // 将previewTab添加到tabWidget 中
    ui->tabWidget->addTab(previewTab, "画面预览");
}

void MainWindow::openCamera()
{
    // 打开摄像头
    m_hcam = Nncam_Open(m_cur.id);
    if (m_hcam)
    {
        // 初始化预览页面
        initPreview();

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

        // 设置摄像头选项，这里设置为RGB字节序，因为QImage使用RGB字节序
        Nncam_put_Option(m_hcam, NNCAM_OPTION_BYTEORDER, 0);

        // 设置是否启用自动曝光
        Nncam_put_AutoExpoEnable(m_hcam, ui->autoExposureCheckBox->isChecked()? 1 : 0);

        // 启动摄像头
        startCamera();

        // 修改按钮
        ui->cameraButton->setText("关闭相机");
        ui->searchCameraButton->setEnabled(false);
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

    // 如果没有图像保存或者用户选择不保存，继续关闭相机并进行内存回收
    if (m_hcam)
    {
        Nncam_Close(m_hcam);
        m_hcam = nullptr;
    }
    delete[] m_pData;
    m_pData = nullptr;

    delete[] m_pWmvRecord;
    m_pWmvRecord = nullptr;

    // 清空图像向量
    imageVector.clear();

    // 删除预览标签页相关的QWidget对象
    delete previewTab;
    previewTab = nullptr;
    delete previewView;
    previewView = nullptr;
    delete previewScene;
    previewScene = nullptr;

    // 移除所有标签页
    while (ui->tabWidget->count() > 0)
    {
        ui->tabWidget->removeTab(0);
    }

    m_timer->stop();
    ui->lblLabel->clear();
    ui->autoExposureCheckBox->setEnabled(false);
    ui->gainSlider->setEnabled(false);
    ui->whileBalanceButton->setEnabled(false);
    ui->temperatureSlider->setEnabled(false);
    ui->tintSlider->setEnabled(false);
    ui->captureButton->setEnabled(false);
    ui->videoButton->setEnabled(false);
    ui->previewComboBox->setEnabled(false);
    ui->previewComboBox->clear();
    ui->captureComboBox->setEnabled(false);
    ui->captureComboBox->clear();
    ui->cameraButton->setText("打开相机");
    ui->searchCameraButton->setEnabled(true);

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
    // 定义三个无符号整型变量uimax，uimin和uidef，用于存储曝光时间的范围和默认值
    unsigned uimax = 0, uimin = 0, uidef = 0;
    // 定义三个无符号短整型变量usmax，usmin和usdef，用于存储曝光增益的范围和默认值
    unsigned short usmax = 0, usmin = 0, usdef = 0;
    // 调用Nncam_get_ExpTimeRange函数获取相机支持的曝光时间范围及默认值，并存储在相应的变量中
    Nncam_get_ExpTimeRange(m_hcam, &uimin, &uimax, &uidef);
    // 设置曝光时间滑块的范围为获取到的范围
    ui->exposureTimeSlider->setRange(uimin, uimax);
    // 调用Nncam_get_ExpoAGainRange函数获取相机支持的曝光增益范围及默认值，并存储在相应的变量中
    Nncam_get_ExpoAGainRange(m_hcam, &usmin, &usmax, &usdef);
    // 设置曝光增益滑块的范围为获取到的范围
    ui->gainSlider->setRange(usmin, usmax);

    // 如果当前模型不是单色相机，则处理温度和色度事件
    if (0 == (m_cur.model->flag & NNCAM_FLAG_MONO))
        handleTempTintEvent();

    // 处理曝光事件
    handleExpoEvent();

    // 尝试以回调模式启动相机，如果成功，则进行以下设置
    if (SUCCEEDED(Nncam_StartPullModeWithCallback(m_hcam, eventCallBack, this)))
    {
        // 启用分辨率选择下拉框
        ui->previewComboBox->setEnabled(true);
        ui->captureComboBox->setEnabled(true);
        // 启用自动曝光复选框
        ui->autoExposureCheckBox->setEnabled(true);
        // 根据相机模型启用自动白平衡按钮
        ui->whileBalanceButton->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        // 启用色温滑块
        ui->temperatureSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        // 启用色度滑块
        ui->tintSlider->setEnabled(0 == (m_cur.model->flag & NNCAM_FLAG_MONO));
        // 启用捕获按钮
        ui->captureButton->setEnabled(true);
        // 启用录像按钮
        ui->videoButton->setEnabled(true);

        // 获取当前自动曝光的状态，并设置复选框的选中状态
        int bAuto = 0;
        Nncam_get_AutoExpoEnable(m_hcam, &bAuto);
        ui->autoExposureCheckBox->setChecked(1 == bAuto);

        // 启动一个定时器，每秒触发一次
        m_timer->start(1000);
    }
    // 如果相机无法以回调模式启动，则关闭相机并显示警告消息
    else
    {
        closeCamera();
        QMessageBox::warning(this, "Warning", "Failed to start camera.");
    }
}

void MainWindow::eventCallBack(unsigned nEvent, void* pCallbackCtx)
{
    MainWindow* pThis = reinterpret_cast<MainWindow*>(pCallbackCtx);
    emit pThis->evtCallback(nEvent);
}

void MainWindow::handleImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Nncam_PullImage(m_hcam, m_pData, 24, &width, &height)))
    {
        QImage image(m_pData, width, height, QImage::Format_RGB888);
        framePixmap = QPixmap::fromImage(image);
        pixmapItem->setPixmap(framePixmap);
        previewView->update();
    }

    if (m_pWmvRecord)
        m_pWmvRecord->WriteSample(m_pData);
}

void MainWindow::handleExpoEvent()
{
    unsigned time = 0;
    unsigned short gain = 0;
    Nncam_get_ExpoTime(m_hcam, &time);
    Nncam_get_ExpoAGain(m_hcam, &gain);

    {
        const QSignalBlocker blocker(ui->exposureTimeSlider);
        ui->exposureTimeSlider->setValue(int(time));
        ui->exposureTimeNumLabel->setText(QString::number(time));
    }
    {
        const QSignalBlocker blocker(ui->gainSlider);
        ui->gainSlider->setValue(int(gain));
        ui->gainNumLabel->setText(QString::number(gain));
    }
}

void MainWindow::handleTempTintEvent()
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

void MainWindow::handleStillImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Nncam_PullStillImage(m_hcam, nullptr, 24, &width, &height))) // peek
    {
        std::vector<uchar> vec(TDIBWIDTHBYTES(width * 24) * height);
        if (SUCCEEDED(Nncam_PullStillImage(m_hcam, &vec[0], 24, &width, &height)))
        {
            QImage image(&vec[0], width, height, QImage::Format_RGB888);
                
            // 创建一个新的标签页
            QWidget *newTab = new QWidget();
            QLabel *imageLabel = new QLabel(newTab);

            // 设置标签页的布局，确保QLabel自适应标签页大小
            QVBoxLayout *layout = new QVBoxLayout(newTab);
            layout->addWidget(imageLabel);
            layout->setContentsMargins(0, 0, 0, 0);
            newTab->setLayout(layout);

            QPixmap pixmap = QPixmap::fromImage(image);
            // QPixmap pixmap = QPixmap::fromImage(image).scaled(newTab->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            // imageLabel->setScaledContents(true);
            imageLabel->setPixmap(pixmap);

            // 将新创建的QImage存储到映射中，并添加标签页到tabWidget
            ui->tabWidget->addTab(newTab, QString("image_") + QString::number(++m_count));
            imageVector.append(image);
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

void MainWindow::on_autoExposureCheckBox_stateChanged(bool state)
{
    if (m_hcam)
    {
        Nncam_put_AutoExpoEnable(m_hcam, state ? 1 : 0);
        ui->exposureTimeSlider->setEnabled(!state);
        ui->gainSlider->setEnabled(!state);
    }
}

void MainWindow::on_exposureTimeSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        ui->exposureTargetNumLabel->setText(QString::number(value));
        if (!ui->autoExposureCheckBox->isChecked())
            Nncam_put_ExpoTime(m_hcam, value);
    }
}

void MainWindow::on_gainSlider_valueChanged(int value)
{
    if (m_hcam)
    {
        ui->gainNumLabel->setText(QString::number(value));
        if (!ui->autoExposureCheckBox->isChecked())
            Nncam_put_ExpoAGain(m_hcam, value);
    }
}

void MainWindow::on_temperatureSlider_valueChanged(int value)
{
    m_temp = value;
    if (m_hcam)
        Nncam_put_TempTint(m_hcam, m_temp, m_tint);
    ui->temperatureNumLabel->setText(QString::number(value));
}

void MainWindow::on_tintSlider_valueChanged(int value)
{
    m_tint = value;
    if (m_hcam)
        Nncam_put_TempTint(m_hcam, m_temp, m_tint);
    ui->tintNumLabel->setText(QString::number(value));
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

void MainWindow::stopRecord()
{
    if (m_pWmvRecord)
    {
        m_pWmvRecord->StopRecord();

        delete m_pWmvRecord;
        m_pWmvRecord = nullptr;
    }
}

void MainWindow::updateMainStyle(QString style)
{
    // QSS文件初始化界面样式
    QFile qss("qss/" + style);
    if(qss.open(QFile::ReadOnly))
    {
        qDebug()<<"qss load";
        QTextStream filetext(&qss);
        QString stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    }
    else
    {
        qDebug() << "qss not load";
        qss.setFileName("/qss/" + style);
        if(qss.open(QFile::ReadOnly))
        {
            qDebug() << "qss load";
            QTextStream filetext(&qss);
            QString stylesheet = filetext.readAll();
            this->setStyleSheet(stylesheet);
            qss.close();
        }
    }
}
