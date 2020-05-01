//============================================================================
// @author      : Thomas Dooms
// @date        : 3/22/20
// @copyright   : BA2 Informatica - Thomas Dooms - University of Antwerp
//============================================================================


#pragma once

#include "../yeelight/scanner.h"
#include "deviceWidget.h"

#include <QtWidgets/QColorDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>

class Window : public QMainWindow
{
    public:
    Window()
    : main(new QWidget), layout(new QGridLayout),
      scanner(std::bind(&Window::device_found, this, std::placeholders::_1))
    {
        main->setLayout(layout);
        setCentralWidget(main);
        show();
    }

    void device_found(std::unique_ptr<yeelight::Device> device)
    {
        layout->addWidget(new DeviceWidget(std::move(device)));
    }

    private:
    QWidget*          main;
    QGridLayout*      layout;
    yeelight::Scanner scanner;
};
