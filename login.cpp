#include<QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include "login.h"
#include "ui_login.h"
#include "mainwindow.h"

Login::Login(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Login)
{
    this->setWindowFlags(Qt::FramelessWindowHint);
    ui->setupUi(this);
    ui->passwordEdit->setEchoMode(QLineEdit::Password);

    QFile qss(":qdarkstyle/dark/darkstyle.qss");

    if(qss.open(QFile::ReadOnly))
    {
        QTextStream filetext(&qss);
        QString stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    }
}

Login::~Login()
{
    delete ui;
}

void Login::on_loginButton_clicked()
{
    if(ui->accountEdit->text() == "gdut" && ui->passwordEdit->text() == "123456"){
        this->close();
        MainWindow *w = new MainWindow;
        w->setWindowTitle(u8"单细胞仪器");
        w->setWindowIcon(QIcon(":/images/images/logo.png"));
        // w->showMaximized();
    }else{
        QMessageBox::warning(this, "Warning", "The account number or password is incorrect.");
        ui->accountEdit->clear();
        ui->passwordEdit->clear();
        ui->accountEdit->setFocus();
    }
}

void Login::on_quitButton_clicked()
{
    this->close();
}

