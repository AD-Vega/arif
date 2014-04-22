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
#include <QMetaType>
#include <QInputDialog>

#include <QDebug>
#include <cassert>

typedef QMap<QString, EstimatorSettings> Presets;
Q_DECLARE_METATYPE(Presets)

static uint fpsUpdateSec = 3;

ArifMainWindow::ArifMainWindow(VideoSourcePlugin* plugin, QWidget* videoControls, QWidget* parent, Qt::WindowFlags flags):
    QMainWindow(parent, flags), sourceControl(videoControls)
{
    settings.plugin = plugin;
    setupUi(this);
    if (sourceControl) {
        sourceControlDock->setWidget(sourceControl);
        sourceControlDock->setVisible(true);
    } else {
        sourceControlDock->setVisible(false);
    }
    // Delay initialization until a later event loop cycle.
    QTimer::singleShot(0, this, SLOT(initialize()));
    QSettings config;
    restoreGeometry(config.value("mainwindow/geometry").toByteArray());
}

void ArifMainWindow::initialize()
{
    QSettings config;
    // Make sure the window is properly shown before restoring docks etc.
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents |
                                QEventLoop::ExcludeSocketNotifiers);
    restoreState(config.value("mainwindow/state").toByteArray());
    sourceControlDock->setVisible((bool)sourceControl);

    // Connect widgets whose connections need to be triggered when settings are read.
    connect(displayCheck, SIGNAL(toggled(bool)),
            videoDock, SLOT(setVisible(bool)));
    connect(displayCheck, SIGNAL(toggled(bool)),
            histogramDock, SLOT(setVisible(bool)));
    connect(displayCheck, SIGNAL(toggled(bool)),
            qualityGraphDock, SLOT(setVisible(bool)));
    connect(clearGraphsButton, SIGNAL(clicked(bool)),
            qualityGraph, SLOT(clear()));
    connect(displayCheck, SIGNAL(toggled(bool)),
            qualityHistogramDock, SLOT(setVisible(bool)));
    connect(clearGraphsButton, SIGNAL(clicked(bool)),
            qualityHistogram, SLOT(clear()));
    connect(shortGraphLength, SIGNAL(valueChanged(int)),
            qualityGraph, SLOT(setShortGraphMaxFrames(int)));

    // Restore settings and clear the FPS display, which is garbage at startup.
    restoreProgramSettings();
    updateFps();

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
    connect(histogramLogarithmicCheck, SIGNAL(toggled(bool)), SLOT(updateSettings()));
    connect(histogramLogarithmicCheck, SIGNAL(toggled(bool)), SLOT(getFrameToRender()));
    connect(markClippedCheck, SIGNAL(toggled(bool)), SLOT(updateSettings()));
    connect(markClippedCheck, SIGNAL(toggled(bool)), SLOT(getFrameToRender()));
    connect(negativeCheck, SIGNAL(toggled(bool)), SLOT(updateSettings()));

    // Prepare the processing pipeline and start displaying frames.
    foreman.reset(new Foreman);
    updateSettings();
    auto reader = settings.plugin->reader();
    connect(reader, SIGNAL(frameReady(SharedRawFrame)),
            foreman.data(), SLOT(takeFrame(SharedRawFrame)));
    connect(reader, SIGNAL(frameReady(SharedRawFrame)),
            SLOT(requestRendering()));
    connect(reader, SIGNAL(frameReady(SharedRawFrame)),
            SLOT(frameReceived()));
    connect(reader, SIGNAL(error(QString)), SLOT(readerError(QString)));
    connect(reader, SIGNAL(atEnd()), SLOT(readerFinished()));
    connect(foreman.data(), SIGNAL(ready()),
            reader, SLOT(readFrame()));
    connect(foreman.data(),
            SIGNAL(frameProcessed(SharedData)),
            SLOT(frameProcessed(SharedData)));
    connect(foreman.data(), SIGNAL(frameProcessed(SharedData)),
            qualityGraph, SLOT(addFrameStats(SharedData)));
    connect(foreman.data(), SIGNAL(frameProcessed(SharedData)),
            qualityHistogram, SLOT(addFrameStats(SharedData)));
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
    } else {
        seekSlider->setVisible(false);
    }

    printActiveThreads();
    auto fpsTimer = new QTimer(this);
    connect(fpsTimer, SIGNAL(timeout()), SLOT(updateFps()));
    fpsTimer->start(1000 * fpsUpdateSec);
}

void ArifMainWindow::requestRendering()
{
    if (++finishedFrameCounter > displayInterval->value()) {
        finishedFrameCounter = 0;
        if (displayCheck->isChecked()) {
            foreman->renderNextFrame();
            qualityGraph->draw();
            qualityHistogram->draw();
        }
    }
}

void ArifMainWindow::frameProcessed(SharedData data)
{
    if (data->doRender && data->completedStages.contains(ProcessingStage::Render)) {
        // Just swap image data with the one currently rendered.
        videoWidget->unusedFrame()->swap(data->renderedFrame);
        videoWidget->swapFrames();
        videoWidget->setDrawnPath(data->paintObjects);
        bool gray = 1 == data->decoded.channels();
        histogramWidget->updateHistograms(data->histograms, gray);
    }
    if (data->stageSuccessful) {
        processedFrames++;
        if (data->completedStages.contains(ProcessingStage::EstimateQuality)) {
            if (settings.filterType == QualityFilterType::MinimumQuality
                    && !data->accepted)
                rejectedFrames++;
            if (acceptanceEntireFileCheck->isChecked())
                entireFileQualities << data->quality;
        }
        if (data->completedStages.contains(ProcessingStage::Decode)) {
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
            if (!decodedImagePixelSize) {
                decodedImagePixelSize = data->decoded.elemSize();
                updateSettings();
            }
        }
    } else {
        missedFrames++;
    }
}

void ArifMainWindow::frameReceived()
{
    receivedFrames++;
}

void ArifMainWindow::frameMissed()
{
    missedFrames++;
}

void ArifMainWindow::updateFps()
{
    double div = fpsUpdateSec;
    receivedLabel->setText(QString::number((int)(receivedFrames / div)));
    receivedFrames = 0;
    processedLabel->setText(QString::number((int)(processedFrames / div)));
    processedFrames = 0;
    missedLabel->setText(QString::number((int)(missedFrames / div)));
    missedFrames = 0;
    rejectedLabel->setText(QString::number((int)(rejectedFrames / div)));
    rejectedFrames = 0;
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
        processButton->setEnabled(false);
        // Reenable once foreman actually finishes.
        foreman->stop();
        qualityGraph->addLine();
    }
    bool haveFile = !checked && !settings.plugin->reader()->isSequential();
    acceptanceEntireFileCheck->setEnabled(haveFile);
}

void ArifMainWindow::on_imageDestinationButton_clicked(bool checked)
{
  auto dirname = QFileDialog::getExistingDirectory(this, "Open directory",
                   imageDestinationDirectory->text());
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
    calculateQualityCheck->setEnabled(!checked);
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

void ArifMainWindow::on_calculateQualityCheck_toggled(bool checked)
{
    filterCheck->setEnabled(checked);
    if (!checked) {
        filterCheck->setChecked(false);
        qualityGraph->addLine();
    }
    acceptanceEntireFileCheck->setEnabled(checked && !settings.plugin->reader()->isSequential());
    updateSettings();
}

void ArifMainWindow::on_estimatorPresetCombo_activated(int index)
{
    estimatorPresetDelete->setEnabled(index != 0);
    if (index == 0) {
        auto name = QInputDialog::getText(this, "Choose preset name", "Preset name:");
        if (!name.isEmpty()) {
            EstimatorSettings s;
            s.noiseSigma = noiseSigmaSpinbox->value();
            s.signalSigma = signalSigmaSpinbox->value();
            estimatorPresetCombo->insertItem(estimatorPresetCombo->count(),
                                             name, QVariant::fromValue(s));
        }
    } else {
        auto s = estimatorPresetCombo->itemData(index).value<EstimatorSettings>();
        signalSigmaSpinbox->setValue(s.signalSigma);
        noiseSigmaSpinbox->setValue(s.noiseSigma);
    }
}

void ArifMainWindow::on_estimatorPresetDelete_clicked(bool checked)
{
    estimatorPresetCombo->blockSignals(true);
    estimatorPresetCombo->removeItem(estimatorPresetCombo->currentIndex());
    estimatorPresetCombo->setCurrentIndex(0);
    estimatorPresetDelete->setEnabled(false);
    estimatorPresetCombo->blockSignals(false);
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
    settings.negative = negativeCheck->isChecked();
    settings.doCrop = cropCheck->isChecked();
    settings.cropWidth = cropWidthBox->value();
    settings.threshold = thresholdSpinbox->value();
    settings.logarithmicHistograms = histogramLogarithmicCheck->isChecked();
    settings.markClipped = markClippedCheck->isChecked();
    settings.estimateQuality = calculateQualityCheck->isChecked();
    settings.estimatorSettings.noiseSigma = noiseSigmaSpinbox->value();
    settings.estimatorSettings.signalSigma = signalSigmaSpinbox->value();
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
    // Pick the worst-case: 16-bit color image.
    int mem = decodedImagePixelSize;
    if (settings.doCrop) {
        mem *= settings.cropWidth * settings.cropWidth;
    } else {
        auto size = videoWidget->getImageSize();
        mem *= size.width() * size.height();
    }
    mem *= settings.filterQueueLength;
    mem /= 1024*1024;
    memoryLabel->setText(QString("%1 Mb").arg(mem));
    if (foreman)
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
        getFrameToRender();
    }
}

void ArifMainWindow::getFrameToRender()
{
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
    config.setValue("processing/negative", negativeCheck->isChecked());
    config.setValue("processing/cropwidth", cropWidthBox->value());
    config.setValue("processing/saveimages", imageDestinationDirectory->text());
    config.setValue("processing/noisesigma", noiseSigmaSpinbox->value());
    config.setValue("processing/signalsigma", signalSigmaSpinbox->value());
    config.setValue("processing/threshold", thresholdSpinbox->value());
    config.setValue("processing/crop", cropCheck->isChecked());
    config.setValue("processing/loghistogram", histogramLogarithmicCheck->isChecked());
    config.setValue("processing/markclipped", markClippedCheck->isChecked());
    config.setValue("processing/estimatequality", calculateQualityCheck->isChecked());
    config.setValue("filtering/choice", filterMinimumQuality->isChecked());
    config.setValue("filtering/minimumquality", minimumQualitySpinbox->value());
    config.setValue("filtering/acceptancerate", acceptanceSpinbox->value());
    config.setValue("filtering/filterqueue", filterQueueSpinbox->value());
    config.setValue("display/shortgraphlength", shortGraphLength->value());
    config.setValue("display/displayenabled", displayCheck->isChecked());

    Presets presets;
    for (int i = 1; i < estimatorPresetCombo->count(); ++i) {
        presets[estimatorPresetCombo->itemText(i)] =
            estimatorPresetCombo->itemData(i).value<EstimatorSettings>();
    }
    config.setValue("processing/estimatorpresets", QVariant::fromValue(presets));
}

void ArifMainWindow::restoreProgramSettings()
{
    QSettings config;
    displayInterval->setValue(config.value("mainwindow/displayinterval", 10).toInt());
    negativeCheck->setChecked(config.value("processing/negative", false).toBool());
    cropWidthBox->setValue(config.value("processing/cropwidth", 100).toInt());
    imageDestinationDirectory->setText(config.value("processing/saveimages").toString());
    noiseSigmaSpinbox->setValue(config.value("processing/noisesigma", 1.0).toDouble());
    signalSigmaSpinbox->setValue(config.value("processing/signalsigma", 4.0).toDouble());
    thresholdSpinbox->setValue(config.value("processing/threshold", 0.0).toDouble());
    cropCheck->setChecked(config.value("processing/crop", true).toBool());
    histogramLogarithmicCheck->setChecked(config.value("processing/loghistogram").toBool());
    markClippedCheck->setChecked(config.value("processing/markclipped").toBool());
    calculateQualityCheck->setChecked(config.value("processing/estimatequality", true).toBool());
    bool choice = config.value("filtering/choice", false).toBool();
    filterMinimumQuality->setChecked(choice);
    minimumQualitySpinbox->setValue(config.value("filtering/minimumquality", 0.0).toDouble());
    acceptanceSpinbox->setValue(config.value("filtering/acceptancerate", 100).toInt());
    filterQueueSpinbox->setValue(config.value("filtering/filterqueue", 10).toInt());
    shortGraphLength->setValue(config.value("display/shortgraphlength", 1000).toInt());
    displayCheck->setChecked(config.value("display/displayenabled", true).toBool());

    Presets presets = config.value("processing/estimatorpresets").value<Presets>();
    estimatorPresetCombo->addItem("Add new preset...");
    for (auto i = presets.constBegin(), end = presets.constEnd(); i != end; ++i) {
        estimatorPresetCombo->addItem(i.key(), QVariant::fromValue(i.value()));
    }
}

static int registerTypes()
{
    qRegisterMetaType<Presets>("Presets");
    qRegisterMetaTypeStreamOperators<Presets>("Presets");
    return 0;
}

static int __register = registerTypes();
