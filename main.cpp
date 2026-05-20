#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 这里必须和你的图标名字一样：asd.ico
    a.setWindowIcon(QIcon("asd.ico"));

    MainWindow w;
    w.show();
    return a.exec();
}