#include <QApplication>
#include <QIcon>
#include "login.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Login w;
    w.setWindowIcon(QIcon(":/images/images/logo.png"));
    w.show();
    return a.exec();
}
