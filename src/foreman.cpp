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

Foreman::FutureData::FutureData(Foreman* parent, QFuture<SharedData> future_):
    future(future_), watcher(new QFutureWatcher<SharedData>)
{
    connect(watcher.data(), SIGNAL(finished()), parent, SLOT(processingComplete()));
    watcher->setFuture(future);
}

bool Foreman::FutureData::operator==(const Foreman::FutureData& other) const
{
    return future == other.future && watcher == other.watcher;
}

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
    if (futures.empty()) {
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
        futures << FutureData(this, QtConcurrent::run(processData, data));
        requestAnotherFrame();
    } else {
        qDebug() << "dropped frame";
    }
}

void Foreman::processingComplete()
{
    foreach (const FutureData& f, futures) {
        if (f.watcher->isFinished()) {
            SharedData d = f.watcher->result();
            futures.removeOne(f);
            if (!d->stageSuccessful) {
                auto previousStage = d->completedStages.last();
                QString msg("Processing stage %1 failed:");
                qDebug() << msg.arg(previousStage) << d->errorMessage;
                if (previousStage == "Save") {
                    qDebug() << "Error writing image, saving disabled.";
                    auto s = new ProcessingSettings;
                    *s = *settings;
                    s->saveImages = false;
                    settings = QSharedPointer<ProcessingSettings>(s);
                }
                requestAnotherFrame();
            }

            if (d->doRender)
                emit frameRendered(&d->renderedFrame, d->histograms);
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
            requestAnotherFrame();
            dataPool << d;
        }
    }
    if (filterQueue.count() >= settings->filterQueueLength)
        flushFilteringQueue();
    if (!started && futures.empty()) {
        flushFilteringQueue();
        emit stopped();
    }
}

// Save images to disk and return them to be put back into foreman's imagePool.
Foreman::FlushReturn
Foreman::flush(QList< Foreman::QueuedImage > queue, int acceptance)
{
    QList<QSharedPointer<cv::Mat>> localPool;
    std::vector<int> option( { CV_IMWRITE_PXM_BINARY, 1 });
    bool success = true;
    qSort(queue);
    int min = queue.count() * (100 - acceptance) / 100;
    for (int i = queue.count() - 1; i >= min; i--) {
        auto& qi = queue.at(i);
        success = success && cv::imwrite(qi.filename.toStdString(), *qi.image, option);
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
    auto p = QThreadPool::globalInstance();
    return p->activeThreadCount() < p->maxThreadCount();
}

bool Foreman::requestAnotherFrame()
{
    if (started && haveIdleThreads()) {
        emit ready();
    }
}
