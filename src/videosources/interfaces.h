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

#ifndef VIDEOSOURCES_INTERFACES_H
#define VIDEOSOURCES_INTERFACES_H

#include <QtPlugin>
#include <QMetaType>
#include <QSharedPointer>
#include <QDateTime>
#include <QGroupBox>
#include <opencv2/core/core.hpp>

class VideoSourcePlugin;
class RawFrame;
class Decoder;
typedef QSharedPointer<RawFrame> SharedRawFrame;
typedef QSharedPointer<Decoder> SharedDecoder;

struct FrameMetaData
{
    void serialize(QDataStream& s);
    void load(QDataStream& s);

    QDateTime timestamp;
    unsigned int frameOfSecond = 0;
};

class RawFrame
{
public:
    virtual ~RawFrame() {}
    virtual SharedRawFrame copy() = 0;
    virtual VideoSourcePlugin* plugin() = 0;

    // Default serialization takes care of metadata.
    virtual void serialize(QDataStream& s);
    virtual void load(QDataStream& s);
    FrameMetaData metaData;
};

class Decoder
{
public:
    virtual ~Decoder() {}
    virtual const cv::Mat decode(RawFrame* in) = 0;
    virtual VideoSourcePlugin* plugin() = 0;
};

class Reader: public QObject
{
    Q_OBJECT

public:
    virtual ~Reader() {}
    virtual bool seek(qint64 frame) = 0;
    virtual bool isSequential() = 0;
    virtual quint64 numberOfFrames() = 0;
    virtual VideoSourcePlugin* plugin() = 0;

protected:
    FrameMetaData makeMetaData();

public slots:
    // Used by non-live sources to throttle reading.
    // Live sources ignore this and emit frameReady()
    // for every frame.
    virtual void readFrame() = 0;

signals:
    void error(QString msg);
    void atEnd();
    void frameReady(SharedRawFrame frame);

private:
    uint previousUnixtime = 0;
    uint frameOfSecond = 0;
};

class VideoSourceConfigurationWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit VideoSourceConfigurationWidget(const QString& title,
                                            QWidget* parent = 0) :
        QGroupBox(title, parent) {}

signals:
    void configurationComplete();
};

class VideoSourcePlugin
{
public:
    virtual ~VideoSourcePlugin() {}
    virtual QString name() = 0;
    virtual QString readableName() = 0;
    virtual VideoSourceConfigurationWidget* createConfigurationWidget() = 0;
    virtual SharedRawFrame createRawFrame() = 0;
    virtual SharedDecoder createDecoder() = 0;
    virtual Reader* reader() = 0;
};

Q_DECLARE_INTERFACE(VideoSourcePlugin,
                    "si.ad-vega.arif.VideoSourcePlugin/0.1");
Q_DECLARE_METATYPE(VideoSourcePlugin*)

#endif
