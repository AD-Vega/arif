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

#ifndef FOREMAN_H
#define FOREMAN_H

#include "processing.h"
#include <QThreadPool>
#include <QFutureWatcher>
#include <QList>

class Foreman: public QObject
{
    Q_OBJECT

    // This struct combines a future with its watcher and automatically
    // connects the parent Foreman to its finished() signal.
    struct FutureData {
        FutureData(Foreman* parent, QFuture<SharedData> future);
        QFuture<SharedData> future;
        QSharedPointer<QFutureWatcher<SharedData>> watcher;
        bool operator==(const FutureData& other) const;
    };

    // Used for acceptance rate quality filtering.
    struct QueuedImage {
        QSharedPointer<cv::Mat> image;
        QString filename;
        float quality;
        bool operator<(const QueuedImage& other) const {
            return quality < other.quality;
        }
    };

    // Nested types to track imagePool and filterQueue.
    typedef QSharedPointer<cv::Mat> SharedCvMat;
    // Return value for the flushing thread.
    typedef QPair<bool, QList<SharedCvMat>> FlushReturn;
    // Watcher type for queueFlushFuture.
    typedef QFutureWatcher<FlushReturn> FlushWatcher;

public:
    // Call updateSettings before use!
    explicit Foreman(QObject* parent = 0);
    bool isStarted();

public slots:
    // Foreman always accepts frames so it can render them,
    // but will not do other processing unless started.
    void start();

    // Returns immediately, a signal is emmited when actually stopped.
    void stop();

    // Updates the shared pointer, which is put into SharedData when
    // starting a new cycle.
    void updateSettings(const ProcessingSettings& settings);

    // Called by main window when a new frame should be shown.
    void renderNextFrame();

    // Invoked when a new frame is ready.
    void takeFrame(SharedRawFrame frame);

private slots:
    // Invoked when a processing stage has completed.
    void processingComplete();

    // Save images in the filtering queue.
    void flushFilteringQueue();

    // Invoked when the images in the filtering queue have been saved.
    // Returns the images to the imagePool.
    void flushComplete();

signals:
    // Emitted when a renderNextFrame() request completes.
    void frameRendered(QImage, QSharedPointer<Histograms> histograms);

    // Emitted when a frame can be taken. Used by non-live sources
    // to throttle data input and avoid framedrop.
    void ready();

    // Emitted when stopping is complete.
    void stopped();

private:
    bool haveIdleThreads();
    bool requestAnotherFrame();
    static FlushReturn flush(QList<QueuedImage> queue, int acceptance);

private:
    bool started = false;
    bool render = false;
    QSharedPointer<ProcessingSettings> settings;
    QList<SharedData> dataPool;
    QList<FutureData> futures;
    QList<QueuedImage> filterQueue;
    QList<SharedCvMat> imagePool; // For filterQueue.
    QFuture<FlushReturn> queueFlushFuture; // Flushing the queue is done in a thread.
    FlushWatcher* flushWatcher;
};

#endif
