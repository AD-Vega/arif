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

#ifndef ARAVIS_H
#define ARAVIS_H

#define QT_STATICPLUGIN

#include "videosources/interfaces.h"
#include <qarvdecoder.h>

class QArvGui;
class QArvCamera;

namespace Aravis
{

class AravisReader;

class AravisSource: public QObject, public VideoSourcePlugin
{
    Q_OBJECT
    Q_INTERFACES(VideoSourcePlugin)

public:
    explicit AravisSource(QObject* parent = 0);
    QString name();
    QString readableName();
    VideoSourceConfigurationWidget* createConfigurationWidget();
    SharedRawFrame createRawFrame();
    SharedDecoder createDecoder();
    Reader* reader();
    QString settingsGroup();
    QString initialize(QString overrideInput = QString{});

    static AravisSource* instance;

private:
    QSize size;
    QScopedPointer<AravisReader> reader_;
    ArvPixelFormat pixfmt = 0;

    friend class AravisSourceConfigWidget;
    friend class AravisFrame;
    friend class AravisReader;
    friend class AravisDecoder;
};

class AravisSourceConfigWidget : public VideoSourceConfigurationWidget
{
    Q_OBJECT

public:
    explicit AravisSourceConfigWidget(QWidget* parent = 0);

private slots:
    void finish();

private:
    void saveConfig();
    void restoreConfig();
    QArvGui* gui;
};

class AravisFrame: public RawFrame
{
public:
    SharedRawFrame copy();
    VideoSourcePlugin* plugin();
    void serialize(QDataStream& s);
    void load(QDataStream& s);

private:
    QByteArray frame;
    friend class AravisDecoder;
    friend class AravisSource;
    friend class AravisReader;
};

class AravisDecoder: public Decoder
{
public:
    AravisDecoder(ArvPixelFormat pixfmt, QSize size);
    const cv::Mat decode(RawFrame* in);
    VideoSourcePlugin* plugin();

private:
    QScopedPointer<QArvDecoder> thedecoder;
};


class AravisReader: public Reader
{
    Q_OBJECT

public:
    bool seek(qint64 frame);
    bool isSequential();
    quint64 numberOfFrames();
    VideoSourcePlugin* plugin();

public slots:
    void readFrame();

private slots:
    void getFrame(QByteArray frame);

private:
    QArvCamera* camera;
    friend class AravisSource;
    friend class AravisSourceConfigWidget;
};

}

#endif
