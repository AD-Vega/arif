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
#include <QMetaType>
#include <QThread>
#include <boost/asio.hpp>
#include <functional>
#include <cstdio>

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
    friend class RawVideoDecoder;
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
    VideoSourcePlugin* plugin();
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
    RawVideoDecoder();
    const cv::Mat decode(RawFrame* in);
    VideoSourcePlugin* plugin();

private:
    QScopedPointer<QArvDecoder> thedecoder;
};

// This class is a thread that runs the asio event loop.
class AsioThread: public QThread
{
    Q_OBJECT

public:
    AsioThread(boost::asio::io_service* service):
        QThread(), serviceptr(service) {}

    void run() {
        serviceptr->run();
    }

private:
    boost::asio::io_service* serviceptr;
};


class RawVideoReader: public Reader
{
    Q_OBJECT

public:
    RawVideoReader(QString filename);
    RawVideoReader(std::FILE* processStream);
    ~RawVideoReader();
    bool seek(qint64 frame);
    bool isSequential();
    quint64 numberOfFrames();
    VideoSourcePlugin* plugin();

public slots:
    void readFrame();

private slots:
    void asyncReadComplete(SharedRawFrame frame, QString error);

private:
    RawVideoReader();
    void setupAsio(int fd);
    typedef std::function<void(const boost::system::error_code&,
                               std::size_t)> asyncHandlerType;

    boost::asio::io_service service;
    boost::asio::posix::stream_descriptor stream;
    boost::asio::io_service::work work;
    QFile file;
    std::FILE* process = nullptr;
    AsioThread asioThread;
    bool live;
    SharedRawFrame currentlyReadFrame;
    asyncHandlerType asyncHandler;
    bool asyncReadRunning = false;
    int requestedFramesCount = 0;
    friend class RawVideoSource;
};

}

#endif
