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
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_captureButton_clicked()
{

}

void MainWindow::on_videoButton_clicked()
{

}

void MainWindow::on_whileBalanceButton_clicked()
{

}

void MainWindow::on_defaultValueButton_clicked()
{

}

void MainWindow::on_searchCameraButton_clicked()
{
    // 如果相机已打开，则关闭相机
    if (mHcam)
    {
//        closeCamera();
        unsigned count = 0;
    }
    else
    {
        // 清空相机列表
        ui->cameraComboBox->clear();
        // 枚举可用相机设备
        NncamDeviceV2 arr[NNCAM_MAX] = { 0 };
//        unsigned count = Nncam_EnumV2(arr);
        unsigned count = 1;
        // 如果没有找到相机，则显示警告信息
        if (0 == count)
            QMessageBox::warning(this, "Warning", "No camera found.");
        // 如果找到一个相机，则获取该相机并添加到相机列表
        else if (1 == count)
        {
            mCur = arr[0];
//            ui->cameraComboBox->addItem(QString::fromWCharArray(arr[0].displayname));
            ui->cameraComboBox->addItem("测试");
            ui->cameraComboBox->setEnabled(true);
            ui->cameraButton->setEnabled(true);
        }
        // 如果找到多个相机，则获取所有相机并添加到相机列表，获取第一个相机
        else
        {
            for (unsigned i = 0; i < count; ++i)
            {
                ui->cameraComboBox->addItem(QString::fromWCharArray(arr[i].displayname));
//                if (0 == i)
//                    ui->cameraComboBox->addItem("测试1");
//                else
//                    ui->cameraComboBox->addItem("测试2");
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
    {
//        openCamera();
        ui->cameraButton->setText("关闭相机");
        ui->searchCameraButton->setEnabled(false);
    }
    else
    {
//        closeCamera();
        ui->cameraButton->setText("打开相机");
        ui->searchCameraButton->setEnabled(true);
    }
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
//        startCamera();
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
//    m_lbl_frame->clear();
    ui->autoExposureCheckBox->setEnabled(false);
    ui->gainSlider->setEnabled(false);
    ui->gainSlider->setEnabled(false);
    ui->whileBalanceButton->setEnabled(false);
    ui->temperatureSlider->setEnabled(false);
    ui->tintSlider->setEnabled(false);
    ui->captureButton->setEnabled(false);
    ui->previewComboBox->setEnabled(false);
    ui->previewComboBox->clear();
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

void handleExpoEvent()
{

}

void handleTempTintEvent()
{

}























