#include <QApplication>
#include "GameWidget.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("JumpAJump");
    a.setOrganizationName("JumpAJump");
    GameWidget w;
    w.show();
    return QApplication::exec();
}
