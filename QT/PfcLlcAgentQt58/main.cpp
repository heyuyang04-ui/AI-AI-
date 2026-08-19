#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("PFC + LLC 智能监控上位机");
    app.setOrganizationName("PfcLlcTeam");

    Widget window;
    window.resize(1360, 820);
    window.show();
    return app.exec();
}
