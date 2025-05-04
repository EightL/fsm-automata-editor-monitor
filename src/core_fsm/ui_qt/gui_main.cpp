// ui_qt/gui_main.cpp
#include <QApplication>
#include "mainwindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    /* 1) Always use Fusion – identical on all platforms,
          and easy to recolour.                                       */
    QApplication::setStyle("Fusion");

    /* 2) Build an explicit light palette and apply it once.
          (If you want a switch later, keep a copy of the old palette
           and swap them on demand.)                                  */
    QPalette light;
    light.setColor(QPalette::Window,           QColor(0xf7,0xf7,0xf7));
    light.setColor(QPalette::WindowText,       Qt::black);
    light.setColor(QPalette::Base,             Qt::white);
    light.setColor(QPalette::AlternateBase,    QColor(0xee,0xee,0xee));
    light.setColor(QPalette::Text,             Qt::black);
    light.setColor(QPalette::Button,           QColor(0xf0,0xf0,0xf0));
    light.setColor(QPalette::ButtonText,       Qt::black);
    light.setColor(QPalette::Highlight,        QColor(0x2b,0x79,0xff));
    light.setColor(QPalette::HighlightedText,  Qt::white);
    qApp->setPalette(light);

    MainWindow w;
    w.show();
    return app.exec();
}