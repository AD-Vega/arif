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

Foreman::Foreman(const ProcessingSettings& settings_, QObject* parent):
    QObject(parent), settings(new ProcessingSettings)
{
    *settings = settings_;
}

void Foreman::start()
{

}

void Foreman::stop()
{

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
    QList<int> finished;
    for (int i = 0; i < futures.count(); i++) {
        if (futures.at(i).future.isFinished())
            finished << i;
    }
    for (int i: finished) {
        SharedData d = futures.at(i).watcher->result();
        auto previousStage = d->completedStages.last();
        // TODO do more than render
        // Always run, even if there are no free threads now,
        // they will appear sooner or later.
        switch (previousStage) {
        case ProcessingStage::Decode:
            if (render) {
                render = false;
                futures << FutureData(this, QtConcurrent::run(RenderStage, d));
            }
            break;
        case ProcessingStage::RenderFrame:
            emit frameRendered(d->renderedFrame, d->histograms);
        }
    }
}

bool Foreman::haveIdleThreads()
{
    auto p = QThreadPool::globalInstance();
    return p->activeThreadCount() < p->maxThreadCount();
}

