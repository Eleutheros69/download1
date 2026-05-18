#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("多线程下载工具");
    a.setOrganizationName("YourCompany");
    MainWindow w;
    w.show();
    return a.exec();
}