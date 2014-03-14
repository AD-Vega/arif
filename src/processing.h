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

#ifndef PROCESSING_H
#define PROCESSING_H

#include "videosources/interfaces.h"
#include <QScopedPointer>
#include <QSharedPointer>
#include <QList>
#include <QRect>
#include <QImage>
#include <QPainterPath>
#include <opencv2/core/core.hpp>

class ProcessingData;
typedef QSharedPointer<ProcessingData> SharedData;

// Call this using QtConcurrent::run().
SharedData processData(SharedData data);

enum class QualityFilterType
{
    None,
    MinimumQuality, // Handled by the saving stage of processing
    AcceptanceRate  // Handled by the foreman
    // Filtering a whole file is handled by main window, which will
    // first use None and track qualities itself, and then do a second
    // pass using MinimumQuality.
};

struct ProcessingSettings {
    // This will never be changed once the source is chosen
    VideoSourcePlugin* plugin;
    // DecodeAndCrop
    uint cropWidth;
    // RenderFrame
    bool markClipped;
    bool computeHistograms;
    bool logarithmicHistograms;
    // EstimateQuality
    double noiseSigma, signalSigma;
    // Save
    bool saveImages;
    QString saveImagesDirectory;
    // Filter
    QualityFilterType filterType;
    double minimumQuality;
    int acceptancePercent;
    int filterQueueLength;
};

struct Histograms {
  float red[256], green[256], blue[256];
};

/*
 * A pointer to this struct is given to a processing stage.
 * A stages are ordered, so each can count on the data from
 * the previous stage. It will fill the data fields that it
 * computes. These structs are reused to avoid memory churn,
 * thus stages shoud reuse cv::Mat memory and similar.
 * reset() will (re)initialize the appropriate fields for reuse,
 * the rest (such as decoder) is Foreman's responsibility.
 */
struct ProcessingData {
    // A stage will use these for error handling.
    bool stageSuccessful;
    QString errorMessage;

    QStringList completedStages;
    // Settings are reference-counted to allow Foreman
    // to change settings for new instances.
    QSharedPointer<ProcessingSettings> settings;
    SharedDecoder decoder;
    SharedRawFrame rawFrame;

    // Decode
    cv::Mat decoded;      // Any format
    cv::Mat decodedFloat; // CV_32FC
    cv::Mat grayscale;    // CV_32FC

    // Crop
    QRect cropArea;
    cv::Rect cvCropArea;

    // EstimateQuality
    cv::Mat blurNoise, blurSignal;
    float quality;

    // RenderFrame
    bool doRender, onlyRender;
    cv::Mat renderTemporary;
    QImage renderedFrame;
    QSharedPointer<Histograms> histograms;
    // Any stage can draw into this when doRender == true.
    QPainterPath painterPath;

    // When using MinimumQuality filtering, the save routine will
    // set this, regardless of whether the image was actually saved.
    // The latter is dependent on whether saving is enabled.
    bool accepted;
    // When using AcceptanceRate filtering, the save routine will
    // make a deep copy of the decoded image for the Foreman, who
    // will swap an unused image with this one.
    QSharedPointer<cv::Mat> cloned;
    QString filename;

    void reset(QSharedPointer<ProcessingSettings> s) {
        completedStages.clear();
        settings = s;
        doRender = false;
        painterPath = QPainterPath();
    }
};

#endif
