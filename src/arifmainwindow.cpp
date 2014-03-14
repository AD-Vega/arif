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
#include <cassert>

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
    connect(filterAcceptanceRate, SIGNAL(toggled(bool)), SLOT(updateSettings()));
    connect(minimumQualitySpinbox, SIGNAL(valueChanged(double)), SLOT(updateSettings()));
    connect(acceptanceSpinbox, SIGNAL(valueChanged(int)), SLOT(updateSettings()));
    connect(filterQueueSpinbox, SIGNAL(valueChanged(int)), SLOT(updateSettings()));
    connect(filterCheck, SIGNAL(toggled(bool)), SLOT(updateSettings()));
    connect(cropWidthButton, SIGNAL(toggled(bool)), videoWidget, SLOT(enableSelection(bool)));
    connect(thresholdButton, SIGNAL(toggled(bool)), videoWidget, SLOT(enableSelection(bool)));
    connect(videoWidget, SIGNAL(selectionComplete(QRect)), SLOT(imageRegionSelected(QRect)));
    connect(thresholdSpinbox, SIGNAL(valueChanged(double)), SLOT(updateSettings()));
    connect(cropCheck, SIGNAL(toggled(bool)), SLOT(updateSettings()));

    // Prepare the processing pipeline and start displaying frames.
    foreman.reset(new Foreman);
    updateSettings();
    auto reader = settings.plugin->reader();
    connect(reader, SIGNAL(frameReady(SharedRawFrame)),
            foreman.data(), SLOT(takeFrame(SharedRawFrame)));
    connect(reader, SIGNAL(frameReady(SharedRawFrame)),
            SLOT(requestRendering()));
    connect(reader, SIGNAL(error(QString)), SLOT(readerError(QString)));
    connect(reader, SIGNAL(atEnd()), SLOT(readerFinished()));
    connect(foreman.data(), SIGNAL(ready()),
            reader, SLOT(readFrame()));
    connect(foreman.data(),
            SIGNAL(frameProcessed(SharedData)),
            SLOT(frameProcessed(SharedData)));
    connect(foreman.data(), SIGNAL(stopped()), SLOT(foremanStopped()));
    // Read a frame and render it. If this is a file, go back to beginning.
    foreman->renderNextFrame();
    reader->readFrame();
    if (!reader->isSequential()) {
        reader->seek(0);
        seekSlider->setEnabled(true);
        seekSlider->setMinimum(0);
        seekSlider->setMaximum(reader->numberOfFrames());
        connect(reader, SIGNAL(frameReady(SharedRawFrame)), SLOT(incrementSlider()));
        acceptanceEntireFileCheck->setEnabled(true);
        QString txt = acceptanceEntireFileCheck->text();
        txt += " (" + tr("%1 frames") + ")";
        txt = txt.arg(reader->numberOfFrames());
        acceptanceEntireFileCheck->setText(txt);
    }

    printActiveThreads();
}

void ArifMainWindow::requestRendering()
{
    if (++finishedFrameCounter > displayInterval->value()) {
        foreman->renderNextFrame();
        finishedFrameCounter = 0;
    }
}

void ArifMainWindow::frameProcessed(SharedData data)
{
    if (data->doRender) {
        // Just swap image data with the one currently rendered.
        videoWidget->unusedFrame()->swap(data->renderedFrame);
        videoWidget->swapFrames();
        videoWidget->setDrawnPath(data->painterPath);
    }
    if (acceptanceEntireFileCheck->isChecked())
        entireFileQualities << data->quality;
    if (!thresholdSamplingArea.isEmpty()) {
        QRect t = thresholdSamplingArea;
        thresholdSamplingArea = QRect();
        cv::Rect ct(t.x(), t.y(), t.width(), t.height());
        cv::Mat_<float> m = data->grayscale(ct);
        m = m.clone().reshape(1, m.total());
        qSort(m);
        // Disregard burnt pixels, so pick the 99% brightest.
        thresholdSpinbox->setValue(m(.99 * m.total()));
    }
}

void ArifMainWindow::on_processButton_toggled(bool checked)
{
    entireFileQualities.clear();
    if (checked) {
        if (acceptanceEntireFileCheck->isChecked()) {
            seekSlider->setValue(0);
            saveImagesCheck->setChecked(false);
            filterCheck->setChecked(false);
        }
        foreman->start();
    } else {
        foreman->stop();
        processButton->setEnabled(false);
        // Reenable once foreman actually finishes.
    }
    bool haveFile = !checked && !settings.plugin->reader()->isSequential();
    acceptanceEntireFileCheck->setEnabled(haveFile);
}

void ArifMainWindow::on_imageDestinationButton_clicked(bool checked)
{
  auto dirname = QFileDialog::getExistingDirectory(this, "Open directory");
  if (!dirname.isNull()) {
    imageDestinationDirectory->setText(dirname);
  }
}

void ArifMainWindow::on_seekSlider_valueChanged(int val)
{
    auto reader = settings.plugin->reader();
    foreman->renderNextFrame();
    reader->seek(val);
    if (!foreman->isStarted()) {
        disconnect(reader, SIGNAL(frameReady(SharedRawFrame)), this, SLOT(incrementSlider()));
        settings.plugin->reader()->readFrame();
        connect(reader, SIGNAL(frameReady(SharedRawFrame)), SLOT(incrementSlider()));
        settings.plugin->reader()->seek(val);
    }
}

void ArifMainWindow::on_acceptanceEntireFileCheck_toggled(bool checked)
{
    saveImagesCheck->setEnabled(!checked);
    filterCheck->setEnabled(!checked);
    seekSlider->setEnabled(!checked);
    auto self = qobject_cast<QWidget*>(acceptanceEntireFileCheck);
    auto acceptance = qobject_cast<QWidget*>(acceptanceSpinbox);
    auto acceptanceLbl = qobject_cast<QWidget*>(acceptanceSpinboxLabel);
    foreach (auto wgtObject, filteringBox->children()) {
        auto wgt = qobject_cast<QWidget*>(wgtObject);
        if (wgt &&
            wgt != self &&
            wgt != acceptance &&
            wgt != acceptanceLbl)
            wgt->setEnabled(!checked);
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
    if (acceptanceEntireFileCheck->isChecked()) {
        if (filterCheck->isChecked()) {
            // Second pass finished.
            entireFileQualities.clear();
            filterCheck->setChecked(false);
            saveImagesCheck->setChecked(false);
            processButton->setChecked(false);
        } else {
            // Start second pass.
            qSort(entireFileQualities);
            int acceptance = acceptanceSpinbox->value();
            int minIdx = entireFileQualities.count() * (100 - acceptance) / 100;
            float minQuality = entireFileQualities.at(minIdx);
            filterMinimumQuality->setChecked(true);
            minimumQualitySpinbox->setValue(minQuality);
            filterCheck->setChecked(true);
            saveImagesCheck->setChecked(true);
            seekSlider->setValue(0);
        }
    } else {
        processButton->setChecked(false);
    }
}

void ArifMainWindow::updateSettings()
{
    settings.computeHistograms = false;
    settings.doCrop = cropCheck->isChecked();
    settings.cropWidth = cropWidthBox->value();
    settings.threshold = thresholdSpinbox->value();
    settings.logarithmicHistograms = false;
    settings.markClipped = false;
    settings.noiseSigma = noiseSigmaSpinbox->value();
    settings.signalSigma = signalSigmaSpinbox->value();
    settings.saveImages = saveImagesCheck->isChecked();
    imageDestinationBox->setEnabled(!settings.saveImages);
    settings.saveImagesDirectory = imageDestinationDirectory->text();
    if (filterCheck->isChecked()) {
        if (filterMinimumQuality->isChecked()) {
            settings.filterType = QualityFilterType::MinimumQuality;
        } else if (filterAcceptanceRate->isChecked()) {
            settings.filterType = QualityFilterType::AcceptanceRate;
        } else {
            assert(false);
        }
    } else {
        settings.filterType = QualityFilterType::None;
    }
    settings.minimumQuality = minimumQualitySpinbox->value();
    settings.acceptancePercent = acceptanceSpinbox->value();
    settings.filterQueueLength = filterQueueSpinbox->value();
    QSize fsize = settings.plugin->frameSize();
    // Pick the worst-case: 16-bit color image.
    int mem = fsize.height() * fsize.width() * 2 * 3;
    mem *= settings.filterQueueLength;
    mem /= 1024*1024;
    memoryLabel->setText(QString("%1 Mb").arg(mem));
    foreman->updateSettings(settings);
}

void ArifMainWindow::incrementSlider()
{
    seekSlider->blockSignals(true);
    seekSlider->setValue(seekSlider->value() + 1);
    seekSlider->blockSignals(false);
}

void ArifMainWindow::printActiveThreads()
{
    qDebug() << "Active threads:" << QThreadPool::globalInstance()->activeThreadCount();
    QTimer::singleShot(3000, this, SLOT(printActiveThreads()));
}

void ArifMainWindow::imageRegionSelected(QRect region)
{
    if (cropWidthButton->isChecked()) {
        cropWidthBox->setValue(qMax(region.width(), region.height()));
        cropWidthButton->setChecked(false);
    }
    if (thresholdButton->isChecked()) {
        thresholdButton->setChecked(false);
        thresholdSamplingArea = region;
        if (!foreman->isStarted()) {
            auto reader = settings.plugin->reader();
            if (reader->isSequential()) {
                foreman->renderNextFrame();
                reader->readFrame();
            } else {
                on_seekSlider_valueChanged(seekSlider->value());
            }
        }
    }
}

void ArifMainWindow::closeEvent(QCloseEvent* event)
{
    saveProgramSettings();
    foreman->stop();
    while (foreman->isStarted())
        QApplication::processEvents();
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
    config.setValue("processing/threshold", thresholdSpinbox->value());
    config.setValue("processing/crop", cropCheck->isChecked());
    config.setValue("filtering/choice", filterMinimumQuality->isChecked());
    config.setValue("filtering/minimumquality", minimumQualitySpinbox->value());
    config.setValue("filtering/acceptancerate", acceptanceSpinbox->value());
    config.setValue("filtering/filterqueue", filterQueueSpinbox->value());
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
    thresholdSpinbox->setValue(config.value("processing/threshold", 0.0).toDouble());
    cropCheck->setChecked(config.value("processing/crop", true).toBool());
    bool choice = config.value("filtering/choice", false).toBool();
    filterMinimumQuality->setChecked(choice);
    minimumQualitySpinbox->setValue(config.value("filtering/minimumquality", 0.0).toDouble());
    acceptanceSpinbox->setValue(config.value("filtering/acceptancerate", 100).toInt());
    filterQueueSpinbox->setValue(config.value("filtering/filterqueue", 10).toInt());
}
