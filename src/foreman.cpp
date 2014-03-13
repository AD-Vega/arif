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

Foreman::FutureData::FutureData(Foreman* parent, QFuture<SharedData> future_):
    future(future_), watcher(new QFutureWatcher<SharedData>)
{
    connect(watcher.data(), SIGNAL(finished()), parent, SLOT(stageComplete()));
    watcher->setFuture(future);
}

bool Foreman::FutureData::operator==(const Foreman::FutureData& other) const
{
    return future == other.future && watcher == other.watcher;
}

Foreman::Foreman(const ProcessingSettings& settings_, QObject* parent):
    QObject(parent), settings(new ProcessingSettings)
{
    *settings = settings_;
}

void Foreman::start()
{
    started = true;
    emit ready();
}

void Foreman::stop()
{
    started = false;
    if (futures.empty())
        emit stopped();
}

void Foreman::updateSettings(const ProcessingSettings& settings)
{

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
            data->settings = settings;
        }
        data->rawFrame = frame;
        futures << FutureData(this, QtConcurrent::run(DecodeStage, data));
    }
}

void Foreman::stageComplete()
{
    foreach (const FutureData& f, futures) {
        if (f.watcher->isFinished()) {
            SharedData d = f.watcher->result();
            futures.removeOne(f);
            if (!d->stageSuccessful) {
                qDebug() << "Processing stage failed:" << d->errorMessage;
                if (started)
                    emit ready();
            }
            auto previousStage = d->completedStages.last();
            // Always run, even if there are no free threads now,
            // they will appear sooner or later.
            switch (previousStage) {
            case ProcessingStage::Decode:
                if (render) {
                    render = false;
                    futures << FutureData(this, QtConcurrent::run(RenderStage, d));
                } else if (started) { // Decoding might have been started for rendering only
                    futures << FutureData(this, QtConcurrent::run(CropStage, d));
                }
                break;
            case ProcessingStage::RenderFrame:
                if (started) {
                    futures << FutureData(this, QtConcurrent::run(CropStage, d));
                }
                emit frameRendered(d->renderedFrame, d->histograms);
                break;
            case ProcessingStage::Crop:
                futures << FutureData(this, QtConcurrent::run(EstimateQualityStage, d));
                break;
            case ProcessingStage::EstimateQuality:
                // Last stage, request next frame
                if (started)
                    emit ready();
                break;
            }
        }
    }
    if (!started && futures.empty())
        emit stopped();
}

bool Foreman::haveIdleThreads()
{
    auto p = QThreadPool::globalInstance();
    return p->activeThreadCount() < p->maxThreadCount();
}

