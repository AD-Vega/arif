/*
    arif, ADV Realtime Image Filtering, a tool for amateur astronomy.
    Copyright (C) 2014 Jure Varlec <jure.varlec@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "arifmainwindow.h"
#include <QTimer>
#include <QSettings>

#include <QDebug>

ArifMainWindow::ArifMainWindow(VideoSourcePlugin* plugin,
                               QWidget* parent,
                               Qt::WindowFlags flags):
    QMainWindow(parent, flags)
{
    settings.plugin = plugin;
    setupUi(this);
    restoreProgramSettings();
    // Delay initialization until a later event loop cycle.
    QTimer::singleShot(0, this, SLOT(initialize()));
}

void ArifMainWindow::initialize()
{
    settings.computeHistograms = false;
    settings.cropWidth = cropWidthBox->value();
    settings.logarithmicHistograms = false;
    settings.markClipped = false;
    foreman.reset(new Foreman(settings));
    settings.plugin->reader();
    connect(settings.plugin->reader(), SIGNAL(frameReady(SharedRawFrame)),
            foreman.data(), SLOT(takeFrame(SharedRawFrame)));
    connect(foreman.data(), SIGNAL(ready()),
            settings.plugin->reader(), SLOT(readFrame()));
    // TODO connect error signal
    connect(foreman.data(),
            SIGNAL(frameRendered(QImage, QSharedPointer<Histograms>)),
            SLOT(displayRenderedFrame(QImage, QSharedPointer<Histograms>)));
    // Read a frame and render it. If this is a file, go back to beginning.
    foreman->renderNextFrame();
    settings.plugin->reader()->readFrame();
    settings.plugin->reader()->seek(0);
}

void ArifMainWindow::displayRenderedFrame(QImage image,
                                          QSharedPointer<Histograms>  histograms)
{
    // The provided image is actually in the foreman's ProcessingData.
    // Just swap it with the one currently rendered.
    videoWidget->unusedFrame()->swap(image);
    videoWidget->swapFrames();
}

void ArifMainWindow::closeEvent(QCloseEvent* event)
{
    saveProgramSettings();
    QWidget::closeEvent(event);
}

void ArifMainWindow::saveProgramSettings()
{
    QSettings config;
    config.setValue("mainwindow/geometry", saveGeometry());
    config.setValue("mainwindow/state", saveState());
    config.setValue("processing/cropwidth", cropWidthBox->value());
    config.setValue("processing/saveimages", imageDestinationDirectory->text());
}

void ArifMainWindow::restoreProgramSettings()
{
    QSettings config;
    restoreGeometry(config.value("mainwindow/geometry").toByteArray());
    restoreState(config.value("mainwindow/state").toByteArray());
    cropWidthBox->setValue(config.value("processing/cropwidth").toInt());
    imageDestinationDirectory->setText(config.value("processing/saveimages").toString());
}
