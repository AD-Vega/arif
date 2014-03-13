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
    QObject(parent)
{
}

bool Foreman::isStarted()
{
    return started;
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
            data->settings = settings;
        }
        data->rawFrame = frame;
        data->doRender = render;
        render = false;
        futures << FutureData(this, QtConcurrent::run(processData, data));

        // Request another if there are free resources.
        if (haveIdleThreads())
            emit ready();
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
                // Request another frame if there are free resources.
                if (started && haveIdleThreads())
                    emit ready();
            }

            if (settings->saveImages) {
                auto& meta = d->rawFrame->metaData;
                QString fnTemplate("%1/frame-%2-%3-q%4.ppm");
                QString filename = fnTemplate
                                   .arg(settings->saveImagesDirectory)
                                   .arg(meta.timestamp.toString("yyyyMMdd-hhmmsszzz"))
                                   .arg(meta.frameOfSecond, 3, 10, QChar('0'))
                                   .arg(d->quality, 0, 'g', 4);
                std::vector<int> option({ CV_IMWRITE_PXM_BINARY, 1 });
                bool written = cv::imwrite(filename.toStdString(),
                                           d->decoded(d->cvCropArea),
                                           option);
                if (!written) {
                    qDebug() << "Error writing image" << filename;
                    qDebug() << "Writing disabled.";
                    auto s = new ProcessingSettings;
                    *s = *settings;
                    s->saveImages = false;
                    settings = QSharedPointer<ProcessingSettings>(s);
                }
            }

            if (d->doRender)
                emit frameRendered(d->renderedFrame, d->histograms);

            // Request another frame if there are free resources.
            if (started && haveIdleThreads())
                emit ready();
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

