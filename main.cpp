#include "clientwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ClientWindow clientWindow;
    clientWindow.show();

    return a.exec();
}
