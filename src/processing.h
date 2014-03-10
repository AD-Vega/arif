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
#include <opencv2/core/core.hpp>

class ProcessingData;

typedef void (*ProcessingFunction) (ProcessingData*);

enum class ProcessingStage
{
    DecodeAndCrop,
    EstimateQuality,
    RenderFrame, // When requested by main window.
};

void DecodeAndCrop(ProcessingData* d);
void EstimateQuality(ProcessingData* d);
void RenderFrame(ProcessingData* d);

struct ProcessingSettings {
    // DecodeAndCrop
    uint cropWidth;
    // RenderFrame
    bool markClipped;
    bool computeHistograms;
    bool logarithmicHistograms;
};

/*
 * A pointer to this struct is given to a processing stage.
 * A stages are ordered, so each can count on the data from
 * the previous stage. It will fill the data fields that it
 * computes. These structs are reused to avoid memory churn,
 * thus stages shoud reuse cv::Mat memory and similar.
 * The Foreman is supposed to (re)initialize the appropriate
 * fields.
 */
struct ProcessingData {
    // A stage will use these for error handling.
    bool stageSuccessful;
    QString errorMessage;

    QList<ProcessingStage> completedStages;
    // Settings are reference-counted to allow Foreman
    // to change settings for new instances.
    QSharedPointer<ProcessingSettings> settings;
    // Foreman will assign the decoder from a pool and will
    // take it back in the end.
    QSharedPointer<Decoder> decoder;

    // Automatically deallocate raw frame when done.
    QScopedPointer<RawFrame> rawFrame;

    // DecodeAndCrop
    cv::Mat decoded;
    cv::Mat cropped; // Can reference same data as ::decoded
    QRect cropArea;

    // EstimateQuality
    float quality;

    // RenderFrame
    QImage renderedFrame;
    QSharedPointer<Histograms> histograms;
};

#endif
