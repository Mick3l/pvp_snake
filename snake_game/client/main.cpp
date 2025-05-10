#include <QApplication>
#include <QTranslator>

#include "main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QTranslator translator;
    translator.load(":/translations/ru.qm");
    app.installTranslator(&translator);

    MainWindow window;
    window.show();
    return app.exec();
}
