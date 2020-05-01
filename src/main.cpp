//============================================================================
// @author      : Thomas Dooms
// @date        : 3/22/20
// @copyright   : BA2 Informatica - Thomas Dooms - University of Antwerp
//============================================================================

#include "frontend/window.h"
#include <QtWidgets/QApplication>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Window window;
    QApplication::exec();
}
