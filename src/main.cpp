#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    
    // Create a window widget
    QWidget w;
    w.setWindowTitle("ICP Project");
    w.resize(400, 300);
    
    // Create a label with your message
    QLabel *label = new QLabel("Hello ICP!");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 24px; font-weight: bold;");
    
    // Create a layout and add the label
    QVBoxLayout *layout = new QVBoxLayout(&w);
    layout->addWidget(label);
    
    // Show the window
    w.show();
    
    // Start the event loop (this keeps the application running)
    return app.exec();
}