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
#include <QMessageBox>
#include <QFileDialog>

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
    // Connect widgets that can update settings.
    connect(noiseSigmaSpinbox, SIGNAL(valueChanged(double)), SLOT(updateSettings()));
    connect(signalSigmaSpinbox, SIGNAL(valueChanged(double)), SLOT(updateSettings()));
    connect(cropWidthBox, SIGNAL(valueChanged(int)), SLOT(updateSettings()));
    connect(saveImagesCheck, SIGNAL(toggled(bool)), SLOT(updateSettings()));

    // Prepare the processing pipeline and start displaying frames.
    foreman.reset(new Foreman);
    updateSettings();
    settings.plugin->reader();
    connect(settings.plugin->reader(), SIGNAL(frameReady(SharedRawFrame)),
            foreman.data(), SLOT(takeFrame(SharedRawFrame)));
    connect(settings.plugin->reader(), SIGNAL(frameReady(SharedRawFrame)),
            SLOT(requestRendering()));
    connect(settings.plugin->reader(), SIGNAL(error(QString)), SLOT(readerError(QString)));
    connect(settings.plugin->reader(), SIGNAL(atEnd()), SLOT(readerFinished()));
    connect(foreman.data(), SIGNAL(ready()),
            settings.plugin->reader(), SLOT(readFrame()));
    connect(foreman.data(),
            SIGNAL(frameRendered(QImage, QSharedPointer<Histograms>)),
            SLOT(displayRenderedFrame(QImage, QSharedPointer<Histograms>)));
    connect(foreman.data(), SIGNAL(stopped()), SLOT(foremanStopped()));
    // Read a frame and render it. If this is a file, go back to beginning.
    foreman->renderNextFrame();
    settings.plugin->reader()->readFrame();
    settings.plugin->reader()->seek(0);
}

void ArifMainWindow::requestRendering()
{
    if (++finishedFrameCounter > displayInterval->value()) {
        foreman->renderNextFrame();
        finishedFrameCounter = 0;
    }
}

void ArifMainWindow::displayRenderedFrame(QImage image,
                                          QSharedPointer<Histograms>  histograms)
{
    // The provided image is actually in the foreman's ProcessingData.
    // Just swap it with the one currently rendered.
    videoWidget->unusedFrame()->swap(image);
    videoWidget->swapFrames();
}

void ArifMainWindow::on_processButton_toggled(bool checked)
{
    if (checked) {
        foreman->start();
    } else {
        foreman->stop();
        processButton->setEnabled(false);
        // Reenable once foreman actually finishes.
    }
}

void ArifMainWindow::on_imageDestinationButton_clicked(bool checked)
{
  auto dirname = QFileDialog::getExistingDirectory(this, "Open directory");
  if (!dirname.isNull()) {
    imageDestinationDirectory->setText(dirname);
  }
}

void ArifMainWindow::foremanStopped()
{
    processButton->setEnabled(true);
}

void ArifMainWindow::readerError(QString error)
{
    processButton->setChecked(false);
    if (error.isNull())
        error = tr("No error message given, video source needs fixing.");
    QMessageBox::critical(this,
                          tr("Video source error"),
                          error);
}

void ArifMainWindow::readerFinished()
{
    processButton->setChecked(false);
}

void ArifMainWindow::updateSettings()
{
    settings.computeHistograms = false;
    settings.cropWidth = cropWidthBox->value();
    settings.logarithmicHistograms = false;
    settings.markClipped = false;
    settings.noiseSigma = noiseSigmaSpinbox->value();
    settings.signalSigma = signalSigmaSpinbox->value();
    settings.saveImages = saveImagesCheck->isChecked();
    imageDestinationBox->setEnabled(!settings.saveImages);
    settings.saveImagesDirectory = imageDestinationDirectory->text();
    foreman->updateSettings(settings);
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
    config.setValue("mainwindow/displayinterval", displayInterval->value());
    config.setValue("processing/cropwidth", cropWidthBox->value());
    config.setValue("processing/saveimages", imageDestinationDirectory->text());
    config.setValue("processing/noisesigma", noiseSigmaSpinbox->value());
    config.setValue("processing/signalsigma", signalSigmaSpinbox->value());
}

void ArifMainWindow::restoreProgramSettings()
{
    QSettings config;
    restoreGeometry(config.value("mainwindow/geometry").toByteArray());
    restoreState(config.value("mainwindow/state").toByteArray());
    displayInterval->setValue(config.value("mainwindow/displayinterval", 1).toInt());
    cropWidthBox->setValue(config.value("processing/cropwidth", 100).toInt());
    imageDestinationDirectory->setText(config.value("processing/saveimages").toString());
    noiseSigmaSpinbox->setValue(config.value("processing/noisesigma", 1.0).toDouble());
    signalSigmaSpinbox->setValue(config.value("processing/signalsigma", 4.0).toDouble());
}
