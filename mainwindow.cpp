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
