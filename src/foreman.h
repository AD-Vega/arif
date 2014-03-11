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

class Foreman: public QObject
{
    Q_OBJECT

public:
    explicit Foreman(const ProcessingSettings& settings,
                     QObject* parent = 0);

public slots:
    // Foreman always accepts frames so it can render them,
    // but will not do other processing unless started.
    // Settings are updated by restarting.
    void start(const ProcessingSettings& settings);

    // Returns immediately, a signal is emmited when actually stopped.
    void stop();

    // Called by main window when a new frame should be shown.
    void renderNextFrame();

    // Invoked when a new frame is ready.
    void takeFrame(QSharedPointer<RawFrame> frame);

signals:
    // Emitted when a renderNextFrame() request completes.
    void frameRendered(QImage, QSharedPointer<Histograms> histograms);

    // Emitted when a frame can be taken. Used by non-live sources
    // to throttle data input and avoid framedrop.
    void ready();

    // Emitted when stopping is complete.
    void stopped();

private:
    bool quit = false;
    bool render = false;
    QThreadPool* threadPool = QThreadPool::globalInstance();
    QSharedPointer<ProcessingSettings> settings;
    QList<QSharedPointer<Decoder>> decoderPool;
    QSharedPointer<RawFrame> cachedFrame;
};

#endif
