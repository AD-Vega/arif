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

#include "videosources/rawvideo.h"
#include <QFormLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
extern "C" {
#include <unistd.h>
#include <sys/stat.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

using namespace RawVideo;

RawVideoSource* RawVideoSource::instance;

RawVideoSource::RawVideoSource(QObject* parent): QObject(parent)
{
    instance = this;
}

SharedRawFrame RawVideoFrame::copy()
{
    SharedRawFrame f(new RawVideoFrame);
    RawVideoFrame* r = static_cast<RawVideoFrame*>(f.data());
    *r = *this;
    return f;
}

VideoSourcePlugin* RawVideoFrame::plugin()
{
    return RawVideoSource::instance;
}

void RawVideoFrame::serialize(QDataStream& s)
{
    s.writeRawData(reinterpret_cast<const char*>(frame.data()),
                   frame.size());
    RawFrame::serialize(s);
}

void RawVideoFrame::load(QDataStream& s)
{
    frame.resize(RawVideoSource::instance->frameBytes);
    s.readRawData(reinterpret_cast<char*>(frame.data()),
                  RawVideoSource::instance->frameBytes);
    RawFrame::load(s);
}

RawVideoDecoder::RawVideoDecoder()
{
    auto s = RawVideoSource::instance;
    auto d = QArvDecoder::makeSwScaleDecoder(s->pixfmt, s->size,
             SWS_FAST_BILINEAR | SWS_BITEXACT);
    thedecoder.reset(d);
}

VideoSourcePlugin* RawVideoDecoder::plugin()
{
    return RawVideoSource::instance;
}

const cv::Mat RawVideoDecoder::decode(RawFrame* in)
{
    auto f = static_cast<RawVideoFrame*>(in);
    auto c = reinterpret_cast<const char*>(f->frame.data());
    thedecoder->decode(QByteArray::fromRawData(c, f->frame.size()));
    return thedecoder->getCvImage();
}

#include <QDebug>
#include <QtConcurrentRun>

static void runEventLoop(boost::asio::io_service* service)
{
    service->run();
    qDebug() << "returning";
}

static void backgroundReading(boost::asio::io_service* service)
{
    service->run();
}

RawVideoReader::RawVideoReader():
    file(RawVideoSource::instance->file),
    stream(service), work(service), asioThread(&service)
{
    file.open(QIODevice::ReadOnly);
    // Always go live unles we have a real file because
    // avoiding framedrop would be too bothersome otherwise.
    live = file.isSequential();
    if (!isSequential()) {
        file.seek(RawVideoSource::instance->headerBytes);
    } else {
        int fd = file.handle();
        uint hbytes = RawVideoSource::instance->headerBytes;
        std::vector<char> buf(hbytes);
        read(fd, buf.data(), hbytes);
        RawVideoSource* s = RawVideoSource::instance;
        currentlyReadFrame= s->createRawFrame();
        RawVideoFrame* f = static_cast<RawVideoFrame*>(currentlyReadFrame.data());
        f->frame.resize(s->frameBytes);
        auto objectptr = this;
        auto islive = live;
        auto frame = currentlyReadFrame;
        asyncHandler = [objectptr, frame, islive]
                       (const boost::system::error_code& err,
                        std::size_t bytes)
        {
            QString msg;
            if (err) {
                msg = "Error reading data:";
                msg += QString::fromStdString(err.message());
            }
            QMetaObject::invokeMethod(objectptr, "asyncReadComplete",
                                      Qt::QueuedConnection,
                                      Q_ARG(SharedRawFrame, frame->copy()),
                                      Q_ARG(QString, msg));
        };
        stream = decltype(stream)(service, fd);
        stream.non_blocking(true);
        // Launch the background thread that will perform reading.
        // The 'work' object will make sure it never finishes.
        asioThread.start();
        // Get the ball rolling
        if (live) {
            RawVideoFrame* f = static_cast<RawVideoFrame*>(currentlyReadFrame.data());
            auto buf = boost::asio::buffer(f->frame.data(), s->frameBytes);
            boost::asio::async_read(stream, buf, asyncHandler);
        }
    }
}

RawVideoReader::~RawVideoReader()
{
    // Abort reading operations before object destruction starts.
    // This also quits the background thread.
    service.stop();
}

VideoSourcePlugin* RawVideoReader::plugin()
{
    return RawVideoSource::instance;
}

bool RawVideoReader::isSequential()
{
    return live || file.isSequential();
}

quint64 RawVideoReader::numberOfFrames()
{
    if (isSequential())
        return 0;
    RawVideoSource* s = RawVideoSource::instance;
    return (file.size() - s->headerBytes) / s->frameBytes;
}

bool RawVideoReader::seek(qint64 frame)
{
    if (isSequential())
        return false;
    RawVideoSource* s = RawVideoSource::instance;
    return file.seek(frame * s->frameBytes + s->headerBytes);
}

void RawVideoReader::readFrame()
{
    RawVideoSource* s = RawVideoSource::instance;
    if (!isSequential()) {
        SharedRawFrame frm = s->createRawFrame();
        RawVideoFrame* f = static_cast<RawVideoFrame*>(frm.data());
        f->frame.resize(s->frameBytes);
        if (file.isOpen()) {
            qint64 status;
            status = file.read(reinterpret_cast<char*>(f->frame.data()), s->frameBytes);
            if (status < 0) {
                emit error("Error reading file.");
            } else if (status < s->frameBytes) {
                emit atEnd();
            } else {
                frm->metaData = makeMetaData();
                emit frameReady(frm);
            }
        } else {
            emit error("Error opening file.");
        }
    } else {
        if (!live) {
            if (!asyncReadRunning) {
                asyncReadRunning = true;
                RawVideoFrame* f = static_cast<RawVideoFrame*>(currentlyReadFrame.data());
                auto buf = boost::asio::buffer(f->frame.data(), s->frameBytes);
                boost::asio::async_read(stream, buf, asyncHandler);
            } else {
                requestedFramesCount++;
            }
        }
    }
}

void RawVideoReader::asyncReadComplete(SharedRawFrame frame, QString err)
{
    if (!err.isNull()) {
        emit error(err);
    } else {
        RawVideoSource* s = RawVideoSource::instance;
        if (live) {
            RawVideoFrame* f = static_cast<RawVideoFrame*>(currentlyReadFrame.data());
            auto buf = boost::asio::buffer(f->frame.data(), s->frameBytes);
            boost::asio::async_read(stream, buf, asyncHandler);
        } else if (requestedFramesCount > 0) {
            --requestedFramesCount;
            RawVideoFrame* f = static_cast<RawVideoFrame*>(currentlyReadFrame.data());
            auto buf = boost::asio::buffer(f->frame.data(), s->frameBytes);
            boost::asio::async_read(stream, buf, asyncHandler);
        } else {
            asyncReadRunning = false;
        }
        emit frameReady(frame);
    }
}

static void populateFormatSelector(QComboBox* sel)
{
    QStringList formats;
    //TODO don't use PIX_FMT_NB once libav is updated
    for (int i = 0; i < PIX_FMT_NB; i++) {
        if (sws_isSupportedInput((PixelFormat)i))
            formats.append(av_get_pix_fmt_name((PixelFormat)i));
    }
    formats.sort();
    sel->clear();
    sel->addItems(formats);
}

RawSourceConfigWidget::RawSourceConfigWidget() :
    VideoSourceConfigurationWidget("Raw video configuration")
{
    auto layout = new QFormLayout(this);
    auto fileInputRow = new QHBoxLayout();
    fileName = new QLineEdit();
    auto openFileDialog = new QPushButton("Open");
    auto icon = QIcon::fromTheme("document-open");
    if (!icon.isNull()) {
        openFileDialog->setText("");
        openFileDialog->setIcon(icon);
    }
    fileInputRow->addWidget(fileName);
    fileInputRow->addWidget(openFileDialog);
    layout->addRow("Input file:", fileInputRow);
    this->connect(openFileDialog, SIGNAL(clicked(bool)), SLOT(getFile()));

    formatSelector = new QComboBox;
    populateFormatSelector(formatSelector);
    layout->addRow("Format:", formatSelector);

    header = new QSpinBox;
    header->setMinimum(0);
    header->setMaximum(1000000);
    layout->addRow("Header bytes:", header);

    width = new QSpinBox();
    height = new QSpinBox();
    width->setMaximum(1000000);
    width->setMinimum(1);
    height->setMaximum(1000000);
    height->setMinimum(1);
    layout->addRow("Width:", width);
    layout->addRow("Height:", height);

    auto finishButton = new QPushButton("Finish");
    layout->addRow(finishButton);
    this->connect(finishButton, SIGNAL(clicked(bool)), SLOT(checkConfig()));
    restoreConfig();
}

void RawSourceConfigWidget::checkConfig()
{
    auto name = fileName->text();
    if (QFileInfo(name).isReadable()) {
        RawVideoSource* s = RawVideoSource::instance;
        s->file = name;
        s->size = QSize(width->value(), height->value());
        s->pixfmt = av_get_pix_fmt(formatSelector->currentText().toAscii());
        s->headerBytes = header->value();
        s->frameBytes = avpicture_get_size(s->pixfmt,
                                           s->size.width(),
                                           s->size.height());
        s->reader_.reset(new RawVideoReader);
        saveConfig();
        emit configurationComplete();
    } else {
        QMessageBox tmp;
        tmp.setWindowTitle("File error");
        tmp.setText("Selected file is not readable.");
        tmp.exec();
    }
}

void RawSourceConfigWidget::getFile()
{
    auto tmp = QFileDialog::getOpenFileName(this,
                                            "Open raw video file",
                                            fileName->text());
    if (!tmp.isNull())
        fileName->setText(tmp);
}

void RawSourceConfigWidget::saveConfig()
{
    QSettings config;
    config.beginGroup("format_" + RawVideoSource::instance->name());
    config.setValue("file", fileName->text());
    config.setValue("pixformat", formatSelector->currentText());
    config.setValue("header_bytes", header->value());
    config.setValue("width", width->value());
    config.setValue("height", height->value());
}

void RawSourceConfigWidget::restoreConfig()
{
    QSettings config;
    config.beginGroup("format_" + RawVideoSource::instance->name());
    fileName->setText(config.value("file").toString());
    int idx = formatSelector->findText(config.value("pixformat").toString());
    formatSelector->setCurrentIndex(idx);
    header->setValue(config.value("header_bytes").toInt());
    width->setValue(config.value("width", 640).toInt());
    height->setValue(config.value("height", 480).toInt());
}

QString RawVideoSource::name()
{
    return "RawVideo";
}

QString RawVideoSource::readableName()
{
    return "Raw video file";
}

VideoSourceConfigurationWidget* RawVideoSource::createConfigurationWidget()
{
    return new RawSourceConfigWidget;
}

SharedDecoder RawVideoSource::createDecoder()
{
    return SharedDecoder(new RawVideoDecoder);
}

SharedRawFrame RawVideoSource::createRawFrame()
{
    return SharedRawFrame(new RawVideoFrame);
}

Reader* RawVideoSource::reader()
{
    return reader_.data();
}

Q_EXPORT_PLUGIN2(RawVideo, RawVideo::RawVideoSource)
Q_IMPORT_PLUGIN(RawVideo)