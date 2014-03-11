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

#ifndef RAWVIDEO_H
#define RAWVIDEO_H

#define QT_STATICPLUGIN

#include "videosources/interfaces.h"
#include <vector>
#include <qarvdecoder.h>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>

namespace RawVideo
{

class RawVideoReader;

class RawVideoSource: public QObject, public VideoSourcePlugin
{
    Q_OBJECT
    Q_INTERFACES(VideoSourcePlugin)

public:
    explicit RawVideoSource(QObject* parent = 0);
    QString name();
    QString readableName();
    VideoSourceConfigurationWidget* createConfigurationWidget();
    SharedRawFrame createRawFrame();
    SharedDecoder createDecoder();
    Reader* reader();
    QSize frameSize();

    static RawVideoSource* instance;

private:
    QSize size;
    QString file;
    QScopedPointer<RawVideoReader> reader_;
    enum PixelFormat pixfmt;
    uint headerBytes;
    int frameBytes;

    friend class RawSourceConfigWidget;
    friend class RawVideoFrame;
    friend class RawVideoReader;
};

class RawSourceConfigWidget : public VideoSourceConfigurationWidget
{
    Q_OBJECT

public:
    explicit RawSourceConfigWidget();

private slots:
    void checkConfig();
    void getFile();

private:
    void saveConfig();
    void restoreConfig();

    QLineEdit* fileName;
    QSpinBox* width, * height, * header;
    QCheckBox* autoContinuousCheckBox;
    QComboBox* formatSelector;
};

class RawVideoFrame: public RawFrame
{
public:
    SharedRawFrame copy();
    VideoSourcePlugin* format();
    void serialize(QDataStream& s);
    void load(QDataStream& s);

private:
    std::vector<uint8_t> frame;
    friend class RawVideoDecoder;
    friend class RawVideoSource;
    friend class RawVideoReader;
};

class RawVideoDecoder: public Decoder
{
public:
    const cv::Mat decode(RawFrame* in);
    VideoSourcePlugin* format();

private:
    QScopedPointer<QArvDecoder> thedecoder;
};

class RawVideoReader: public Reader
{
    Q_OBJECT

public:
    RawVideoReader();
    bool seek(qint64 frame);
    bool isSequential();
    quint64 numberOfFrames();
    VideoSourcePlugin* format();

public slots:
    void readFrame();

private:
    QFile file;
    bool live, sequential;
    friend class RawVideoSource;
};

}

#endif
