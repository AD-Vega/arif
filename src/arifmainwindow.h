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

#ifndef ARIFMAINWINDOW_H
#define ARIFMAINWINDOW_H

#include "videosources/interfaces.h"
#include "ui_arifmainwindow.h"
#include "foreman.h"

class ArifMainWindow : public QMainWindow, public Ui::arifMainWindow
{
    Q_OBJECT

public:
    explicit ArifMainWindow(VideoSourcePlugin* plugin,
                            QWidget* videoControls = 0,
                            QString settingsFile = QString{},
                            QString destinationDir = QString{},
                            QWidget* parent = 0,
                            Qt::WindowFlags flags = Qt::Widget);

private slots:
    void initialize();
    void requestRendering();
    void frameProcessed(SharedData data);
    void frameReceived();
    void frameMissed();
    void updateFps();
    void foremanStopped();
    void readerError(QString error);
    void readerFinished();
    void updateSettings();
    void incrementSlider();
    void imageRegionSelected(QRect region);
    void getFrameToRender();

private slots: // UI widget events
    void on_processButton_toggled(bool checked);
    void on_imageDestinationButton_clicked(bool checked);
    void on_seekSlider_valueChanged(int val);
    void on_acceptanceEntireFileCheck_toggled(bool checked);
    void on_calculateQualityCheck_toggled(bool checked);
    void on_estimatorPresetCombo_activated(int index);
    void on_estimatorPresetDelete_clicked(bool checked);
    void on_exportSettingsButton_clicked(bool checked);

private:
    void closeEvent(QCloseEvent* event);
    void saveProgramSettings(QString filename = QString{});
    void restoreProgramSettings(QString filename = QString{});

private:
    ProcessingSettings settings;
    QScopedPointer<Foreman> foreman;
    int finishedFrameCounter = 0;
    QList<float> entireFileQualities;
    QRect thresholdSamplingArea;
    int decodedImagePixelSize = 0;
    uint receivedFrames = 0;
    uint processedFrames = 0;
    uint missedFrames = 0;
    uint rejectedFrames = 0;
    bool batchMode;
    QWidget* sourceControl;
};

#endif
