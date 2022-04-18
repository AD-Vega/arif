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

#include "foreman.h"
#include <QtConcurrentRun>
#include <opencv2/highgui/highgui.hpp>

Foreman::Foreman(QObject* parent):
    QObject(parent), flushWatcher(new FlushWatcher(this))
{
}

bool Foreman::isStarted()
{
    return started;
}

void Foreman::start()
{
    started = true;
    requestAnotherFrame();
}

void Foreman::stop()
{
    started = false;
    if (runningJobs == 0) {
        flushFilteringQueue();
        emit stopped();
    }
}

void Foreman::updateSettings(const ProcessingSettings& settings_)
{
    settings = QSharedPointer<ProcessingSettings>(new ProcessingSettings);
    *settings = settings_;
}

void Foreman::renderNextFrame()
{
    render = true;
}

void Foreman::takeFrame(SharedRawFrame frame)
{
    // Discard frame if no free threads
    if ((started || render) && haveIdleThreads()) {
        SharedData data;
        if (!dataPool.empty()) {
            data = dataPool.takeLast();
            data->reset(settings);
        } else {
            data = SharedData(new ProcessingData);
            data->decoder = SharedDecoder(settings->plugin->createDecoder());
            data->reset(settings);
        }
        data->rawFrame = frame;
        data->doRender = render;
        data->onlyRender = render && !started;
        render = false;
        ProcessWatcher* watcher;
        if (!futureWatcherPool.isEmpty()) {
            watcher = futureWatcherPool.takeLast();
        } else {
            watcher = new ProcessWatcher(this);
            connect(watcher, SIGNAL(finished()), SLOT(processingComplete()));
        }
        watcher->setFuture(QtConcurrent::run(processData, data));
        runningJobs++;
        requestAnotherFrame();
    } else {
        emit frameMissed();
    }
}

void Foreman::processingComplete()
{
    auto watcher = static_cast<ProcessWatcher*>(sender());
    SharedData d = watcher->result();
    if (!d->stageSuccessful) {
        auto previousStage = d->completedStages.last();
        QString msg("Processing stage %1 failed: %2");
        qDebug() << msg.arg(d->exception.stageName, d->exception.errorMessage);
        if (previousStage == ProcessingStage::Save) {
            qDebug() << "Error writing image, saving disabled.";
            auto s = new ProcessingSettings;
            *s = *settings;
            s->saveImages = false;
            settings = QSharedPointer<ProcessingSettings>(s);
        }
    } else {
        if (d->settings->saveImages &&
                d->settings->filterType == QualityFilterType::AcceptanceRate) {
            SharedCvMat tmp;
            if (!imagePool.empty())
                tmp = imagePool.takeLast();
            else
                tmp = QSharedPointer<cv::Mat>(new cv::Mat);
            tmp.swap(d->cloned);
            QueuedImage qi;
            qi.image = tmp;
            qi.filename = d->filename;
            qi.quality = d->quality;
            filterQueue << qi;
        }
    }
    emit frameProcessed(d);
    futureWatcherPool << watcher;
    dataPool << d;
    runningJobs--;

    if (filterQueue.count() >= settings->filterQueueLength)
        flushFilteringQueue();
    if (!started && runningJobs == 0) {
        flushFilteringQueue();
        emit stopped();
    } else {
        requestAnotherFrame();
    }
}

// Save images to disk and return them to be put back into foreman's imagePool.
Foreman::FlushReturn
Foreman::flush(QList< Foreman::QueuedImage > queue, int acceptance)
{
    QList<QSharedPointer<cv::Mat>> localPool;
    bool success = true;
    std::sort(queue.begin(), queue.end());
    int min = queue.count() * (100 - acceptance) / 100;
    for (int i = queue.count() - 1; i >= min; i--) {
        auto& qi = queue.at(i);
        success = success && saveImage(*qi.image, qi.filename);
        localPool << qi.image;
    }
    return qMakePair(success, localPool);
}

void Foreman::flushFilteringQueue()
{
    if (queueFlushFuture.isRunning())
        return;
    queueFlushFuture = QtConcurrent::run(flush, filterQueue, settings->acceptancePercent);
    filterQueue.clear();
    connect(flushWatcher, SIGNAL(finished()), SLOT(flushComplete()));
    flushWatcher->setFuture(queueFlushFuture);
}

void Foreman::flushComplete()
{
    disconnect(flushWatcher, SIGNAL(finished()), this, SLOT(flushComplete()));
    auto val = flushWatcher->result();
    imagePool.append(val.second);
    if (!val.first) {
        qDebug() << "Error writing images, saving disabled.";
        auto s = new ProcessingSettings;
        *s = *settings;
        s->saveImages = false;
        settings = QSharedPointer<ProcessingSettings>(s);
    }
    // Drop references etc.
    queueFlushFuture = QFuture<FlushReturn>();
}

bool Foreman::haveIdleThreads()
{
    /*
     * The runningJobs counter and the actual number of active threads are
     * out of sync because threads can complete while we are busy with other stuff.
     * The runningJobs counter is important because it measures the resources
     * that have not yet been returned into their pools.
     * Therefore, we check both that there are actual free threads and that
     * there is not too much overcommit of resources.
     */
    auto p = QThreadPool::globalInstance();
    return p->activeThreadCount() < p->maxThreadCount() &&
           (int)runningJobs < 2 * p->maxThreadCount();
}

void Foreman::requestAnotherFrame()
{
    if (started && haveIdleThreads()) {
        emit ready();
    }
}
