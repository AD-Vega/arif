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

#ifndef QARVVIDEO_H
#define QARVVIDEO_H

#define QT_STATICPLUGIN

#include "videosources/interfaces.h"
#include <qarvrecordedvideo.h>
#include <QLineEdit>

namespace QArvVideo
{

class QArvVideoReader;

class QArvVideoSource: public QObject, public VideoSourcePlugin
{
    Q_OBJECT
    Q_INTERFACES(VideoSourcePlugin)

public:
    explicit QArvVideoSource(QObject* parent = 0);
    QString name();
    QString readableName();
    VideoSourceConfigurationWidget* createConfigurationWidget();
    SharedRawFrame createRawFrame();
    SharedDecoder createDecoder();
    Reader* reader();

    static QArvVideoSource* instance;

private:
    void frameDestroyed(QByteArray frameData);

    QSize size;
    QScopedPointer<QArvVideoReader> reader_;
    QList<QByteArray> framePool;
    int frameBytes;

    friend class QArvSourceConfigWidget;
    friend class QArvVideoFrame;
    friend class QArvVideoReader;
    friend class QArvVideoDecoder;
};

class QArvSourceConfigWidget : public VideoSourceConfigurationWidget
{
    Q_OBJECT

public:
    explicit QArvSourceConfigWidget();

private slots:
    void checkConfig();
    void getFile();

private:
    void saveConfig();
    void restoreConfig();

    QLineEdit* fileName;
};

class QArvVideoFrame: public RawFrame
{
public:
    ~QArvVideoFrame();
    SharedRawFrame copy();
    VideoSourcePlugin* plugin();
    void serialize(QDataStream& s);
    void load(QDataStream& s);

private:
    QByteArray frame;
    friend class QArvVideoDecoder;
    friend class QArvVideoSource;
    friend class QArvVideoReader;
};

class QArvVideoDecoder: public Decoder
{
public:
    QArvVideoDecoder();
    const cv::Mat decode(RawFrame* in);
    VideoSourcePlugin* plugin();

private:
    QScopedPointer<QArvDecoder> thedecoder;
};

class QArvVideoReader: public Reader
{
    Q_OBJECT

public:
    QArvVideoReader(QString filename);
    bool seek(qint64 frame);
    bool isSequential();
    quint64 numberOfFrames();
    VideoSourcePlugin* plugin();

public slots:
    void readFrame();

private:
    QArvRecordedVideo qarvVideo;
    friend class QArvVideoSource;
    friend class QArvSourceConfigWidget;
    friend class QArvVideoDecoder;
};

}

#endif
