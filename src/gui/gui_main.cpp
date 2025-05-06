/**
 * @file   gui_main.cpp
 * @brief  Main entry point for the FSM Editor GUI application.
 *         Initializes the Qt application, sets up styling, and creates the main window.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */
#include <QApplication>
#include "mainwindow/mainwindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setStyle("Fusion");

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
