#include <QMessageBox>
#include <QTimer>
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , mHcam(nullptr)
    , mTimer(new QTimer(this))
    , mImgWidth(0), mImgHeight(0), mPData(nullptr)
    , mRes(0), mTemp(NNCAM_TEMP_DEF), mTint(NNCAM_TINT_DEF), mCount(0)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->temperatureNumLabel->setText(QString::number(NNCAM_TEMP_DEF));
    ui->tintNumLabel->setText(QString::number(NNCAM_TINT_DEF));
    ui->temperatureSlider->setRange(NNCAM_TEMP_MIN, NNCAM_TEMP_MAX);
    ui->temperatureSlider->setValue(NNCAM_TEMP_DEF);
    ui->tintSlider->setRange(NNCAM_TINT_MIN, NNCAM_TINT_MAX);
    ui->tintSlider->setValue(NNCAM_TINT_DEF);

    connect(this, &MainWindow::evtCallback, this, [this](unsigned nEvent)
    {
        if (mHcam)
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

    connect(mTimer, &QTimer::timeout, this, [this]()
    {
        unsigned nFrame = 0, nTime = 0, nTotalFrame = 0;
        if (mHcam && SUCCEEDED(Nncam_get_FrameRate(mHcam, &nFrame, &nTime, &nTotalFrame)) && (nTime > 0))
            ui->lblLabel->setText(QString::asprintf("%u, fps = %.1f", nTotalFrame, nFrame * 1000.0 / nTime));
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent*)
{
    closeCamera();
}

void MainWindow::on_captureButton_clicked()
{
//    if (mHcam)
//    {
//        if (0 == mCur.model->still)    // not support still image capture
//        {
//            if (mPData)
//            {
//                QImage image(mPData, mImgWidth, mImgHeight, QImage::Format_RGB888);
//                image.save(QString::asprintf("demoqt_%u.jpg", ++mCount));
//            }
//        }
//        else
//        {
//            QMenu menu;
//            for (unsigned i = 0; i < mCur.model->still; ++i)
//            {
//                menu.addAction(QString::asprintf("%u*%u", mCur.model->res[i].width, mCur.model->res[i].height), this, [this, i](bool)
//                               {
//                                   Nncam_Snap(mCcam, i);
//                               });
//            }
//            menu.exec(mapToGlobal(ui->captureButton->pos()));
//        }
//    }
}

void MainWindow::on_videoButton_clicked()
{

}

void MainWindow::on_whileBalanceButton_clicked()
{
    Nncam_AwbOnce(mHcam, nullptr, nullptr);
}

void MainWindow::on_defaultValueButton_clicked()
{

}

void MainWindow::on_searchCameraButton_clicked()
{
    // 如果相机已打开，则关闭相机
    if (mHcam)
    {
        closeCamera();
        unsigned count = 0;
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
            mCur = arr[0];
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
            mCur = arr[0];
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

void MainWindow::openCamera()
{
    // 打开摄像头
    mHcam = Nncam_Open(mCur.id);
    if (mHcam)
    {
        // 获取摄像头的分辨率信息
        Nncam_get_eSize(mHcam, (unsigned*)&mRes);
        // 获取当前分辨率下的图像宽度和高度
        mImgWidth = mCur.model->res[mRes].width;
        mImgHeight = mCur.model->res[mRes].height;
        // 更新一个previewComboBox下拉列表组件（分辨率）
        // 创建一个信号阻止器对象，用于暂时阻止ui->previewComboBox的信号发送，函数结束时销毁
        const QSignalBlocker blocker(ui->previewComboBox);
        ui->previewComboBox->clear();
        for (unsigned i = 0; i < mCur.model->preview; ++i)
            ui->previewComboBox->addItem(QString::asprintf("%u*%u", mCur.model->res[i].width, mCur.model->res[i].height));
        ui->previewComboBox->setCurrentIndex(mRes);
        ui->previewComboBox->setEnabled(true);
        // 设置摄像头选项，这里设置为RGB字节序，因为QImage使用RGB字节序
        Nncam_put_Option(mHcam, NNCAM_OPTION_BYTEORDER, 0);
        // 设置是否启用自动曝光
        Nncam_put_AutoExpoEnable(mHcam, ui->autoExposureCheckBox->isChecked()? 1 : 0);
        // 启动摄像头
        startCamera();
        // 修改按钮
        ui->cameraButton->setText("关闭相机");
        ui->searchCameraButton->setEnabled(false);
    }
}

void MainWindow::closeCamera()
{
    if (mHcam)
    {
        Nncam_Close(mHcam);
        mHcam = nullptr;
    }
    delete[] mPData;
    mPData = nullptr;

    mTimer->stop();
    ui->lblLabel->clear();
    ui->autoExposureCheckBox->setEnabled(false);
    ui->gainSlider->setEnabled(false);
    ui->gainSlider->setEnabled(false);
    ui->whileBalanceButton->setEnabled(false);
    ui->temperatureSlider->setEnabled(false);
    ui->tintSlider->setEnabled(false);
    ui->captureButton->setEnabled(false);
    ui->previewComboBox->setEnabled(false);
    ui->previewComboBox->clear();
    ui->cameraButton->setText("打开相机");
    ui->searchCameraButton->setEnabled(true);
}

void MainWindow::startCamera()
{
    // 检查mPData指针是否有效，如果是，则释放内存
    if (mPData)
    {
        delete[] mPData;
        mPData = nullptr;
    }
    // 重新分配内存用于存储图像数据，TDIBWIDTHBYTES是一个宏，用于计算图像宽度所需的字节数
    mPData = new uchar[TDIBWIDTHBYTES(mImgWidth * 24) * mImgHeight];
    // 定义三个无符号整型变量uimax，uimin和uidef，用于存储曝光时间的范围和默认值
    unsigned uimax = 0, uimin = 0, uidef = 0;
    // 定义三个无符号短整型变量usmax，usmin和usdef，用于存储曝光增益的范围和默认值
    unsigned short usmax = 0, usmin = 0, usdef = 0;
    // 调用Nncam_get_ExpTimeRange函数获取相机支持的曝光时间范围及默认值，并存储在相应的变量中
    Nncam_get_ExpTimeRange(mHcam, &uimin, &uimax, &uidef);
    // 设置曝光时间滑块的范围为获取到的范围
    ui->exposureTimeSlider->setRange(uimin, uimax);
    // 调用Nncam_get_ExpoAGainRange函数获取相机支持的曝光增益范围及默认值，并存储在相应的变量中
    Nncam_get_ExpoAGainRange(mHcam, &usmin, &usmax, &usdef);
    // 设置曝光增益滑块的范围为获取到的范围
    ui->gainSlider->setRange(usmin, usmax);

    // 如果当前模型不是单色相机，则处理温度和色度事件
    if (0 == (mCur.model->flag & NNCAM_FLAG_MONO))
        handleTempTintEvent();

    // 处理曝光事件
    handleExpoEvent();

    // 尝试以回调模式启动相机，如果成功，则进行以下设置
    if (SUCCEEDED(Nncam_StartPullModeWithCallback(mHcam, eventCallBack, this)))
    {
        // 启用分辨率选择下拉框
        ui->previewComboBox->setEnabled(true);
        // 启用自动曝光复选框
        ui->autoExposureCheckBox->setEnabled(true);
        // 根据相机模型启用自动白平衡按钮
        ui->whileBalanceButton->setEnabled(0 == (mCur.model->flag & NNCAM_FLAG_MONO));
        // 启用色温滑块
        ui->temperatureSlider->setEnabled(0 == (mCur.model->flag & NNCAM_FLAG_MONO));
        // 启用色度滑块
        ui->tintSlider->setEnabled(0 == (mCur.model->flag & NNCAM_FLAG_MONO));
        // 启用捕获按钮
        ui->captureButton->setEnabled(true);

        // 获取当前自动曝光的状态，并设置复选框的选中状态
        int bAuto = 0;
        Nncam_get_AutoExpoEnable(mHcam, &bAuto);
        ui->autoExposureCheckBox->setChecked(1 == bAuto);

        // 启动一个定时器，每秒触发一次
        mTimer->start(1000);
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
    if (SUCCEEDED(Nncam_PullImage(mHcam, mPData, 24, &width, &height)))
    {
        QImage image(mPData, width, height, QImage::Format_RGB888);
        QImage newimage = image.scaled(ui->videoLabel->width(), ui->videoLabel->height(), Qt::KeepAspectRatio, Qt::FastTransformation);
        ui->videoLabel->setPixmap(QPixmap::fromImage(newimage));
    }
}

void MainWindow::handleExpoEvent()
{
    unsigned time = 0;
    unsigned short gain = 0;
    Nncam_get_ExpoTime(mHcam, &time);
    Nncam_get_ExpoAGain(mHcam, &gain);

    const QSignalBlocker blocker1(ui->exposureTimeSlider);
    ui->exposureTimeSlider->setValue(int(time));
    ui->exposureTimeNumLabel->setText(QString::number(time));

    const QSignalBlocker blocker2(ui->gainSlider);
    ui->gainSlider->setValue(int(gain));
    ui->gainNumLabel->setText(QString::number(gain));
}

void MainWindow::handleTempTintEvent()
{
    int nTemp = 0, nTint = 0;
    if (SUCCEEDED(Nncam_get_TempTint(mHcam, &nTemp, &nTint)))
    {
        const QSignalBlocker blocker1(ui->temperatureSlider);
        ui->temperatureSlider->setValue(nTemp);
        ui->temperatureNumLabel->setText(QString::number(nTemp));

        const QSignalBlocker blocker2(ui->tintSlider);
        ui->tintSlider->setValue(nTint);
        ui->tintNumLabel->setText(QString::number(nTint));
    }
}

void MainWindow::handleStillImageEvent()
{
    unsigned width = 0, height = 0;
    if (SUCCEEDED(Nncam_PullStillImage(mHcam, nullptr, 24, &width, &height))) // peek
    {
        std::vector<uchar> vec(TDIBWIDTHBYTES(width * 24) * height);
        if (SUCCEEDED(Nncam_PullStillImage(mHcam, &vec[0], 24, &width, &height)))
        {
            QImage image(&vec[0], width, height, QImage::Format_RGB888);
            image.save(QString::asprintf("demoqt_%u.jpg", ++mCount));
        }
    }
}

void MainWindow::on_previewComboBox_currentIndexChanged(int index)
{
    if (mHcam)
        Nncam_Stop(mHcam);

    mRes = index;
    mImgWidth = mCur.model->res[index].width;
    mImgHeight = mCur.model->res[index].height;

    if (mHcam)
    {
        Nncam_put_eSize(mHcam, static_cast<unsigned>(mRes));
        startCamera();
    }
}

void MainWindow::on_autoExposureCheckBox_stateChanged(bool state)
{
    if (mHcam)
    {
        Nncam_put_AutoExpoEnable(mHcam, state ? 1 : 0);
        ui->exposureTimeSlider->setEnabled(!state);
        ui->gainSlider->setEnabled(!state);
    }
}

void MainWindow::on_exposureTimeSlider_valueChanged(int value)
{
    if (mHcam)
    {
        ui->exposureTargetNumLabel->setText(QString::number(value));
        if (!ui->autoExposureCheckBox->isChecked())
            Nncam_put_ExpoTime(mHcam, value);
    }
}

void MainWindow::on_gainSlider_valueChanged(int value)
{
    if (mHcam)
    {
        ui->gainNumLabel->setText(QString::number(value));
        if (!ui->autoExposureCheckBox->isChecked())
            Nncam_put_ExpoAGain(mHcam, value);
    }
}

void MainWindow::on_temperatureSlider_valueChanged(int value)
{
    mTemp = value;
    if (mHcam)
        Nncam_put_TempTint(mHcam, mTemp, mTint);
    ui->temperatureNumLabel->setText(QString::number(value));
}

void MainWindow::on_tintSlider_valueChanged(int value)
{
    mTint = value;
    if (mHcam)
        Nncam_put_TempTint(mHcam, mTemp, mTint);
    ui->tintNumLabel->setText(QString::number(value));
}
