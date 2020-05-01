//============================================================================
// @author      : Thomas Dooms
// @date        : 3/25/20
// @copyright   : BA2 Informatica - Thomas Dooms - University of Antwerp
//============================================================================


#pragma once

#include "../yeelight/device.h"
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <iostream>

class DeviceWidget : public QWidget
{
    public:
    explicit DeviceWidget(std::unique_ptr<yeelight::Device> lamp)
    : device(std::move(lamp)), button(new QPushButton), layout(new QVBoxLayout),
      brightness(new QSlider(Qt::Horizontal)), red(new QSlider(Qt::Horizontal)),
      green(new QSlider(Qt::Horizontal)), blue(new QSlider(Qt::Horizontal))
    {
        button->setEnabled(false);

        brightness->setEnabled(false);
        brightness->setRange(1, 100);
        brightness->setTracking(false);

        red->setEnabled(false);
        red->setRange(0, 255);
        red->setTracking(false);

        green->setEnabled(false);
        green->setRange(0, 255);
        green->setTracking(false);

        blue->setEnabled(false);
        blue->setRange(0, 255);
        blue->setTracking(false);

        device->when_connected([&]() { button->setEnabled(true); });
        device->when_disconnected([&]() { button->setEnabled(false); });


        device->when_updated([&]() {
            button->setText(QString::fromStdString(device->get_name().first));

            brightness->setEnabled(true);
            red->setEnabled(true);
            green->setEnabled(true);
            blue->setEnabled(true);

            brightness->setValue(device->get_brightness().first);
        });

        layout->addWidget(button);
        layout->addWidget(brightness);

        layout->addWidget(red);
        layout->addWidget(green);
        layout->addWidget(blue);
        setLayout(layout);

        const auto color = [&]([[maybe_unused]] int _) {
            const auto r = red->value();
            const auto g = green->value();
            const auto b = blue->value();
            device->set_rgb_color(dot::color(r, g, b));
        };

        const auto bright = [&](int brightness) { device->set_brightness(brightness); };

        connect(button, &QPushButton::pressed, std::bind(&yeelight::Device::toggle, device.get()));
        connect(brightness, &QSlider::valueChanged, bright);

        connect(red, &QSlider::valueChanged, color);
    }

    private:
    std::unique_ptr<yeelight::Device> device;

    QPushButton* button;
    QVBoxLayout* layout;
    QSlider*     brightness;

    QSlider* red;
    QSlider* green;
    QSlider* blue;
};
