#include <QMessageBox>
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
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

