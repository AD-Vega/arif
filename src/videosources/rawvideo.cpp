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

RawVideoFrame::~RawVideoFrame()
{
    RawVideoSource::instance->frameDestroyed(frame);
}

SharedRawFrame RawVideoFrame::copy()
{
    SharedRawFrame f = RawVideoSource::instance->createRawFrame();
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
    s.writeRawData(frame.constData(), frame.size());
    RawFrame::serialize(s);
}

void RawVideoFrame::load(QDataStream& s)
{
    frame.resize(RawVideoSource::instance->frameBytes);
    s.readRawData(frame.data(), RawVideoSource::instance->frameBytes);
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
    auto c = f->frame.constData();
    thedecoder->decode(QByteArray::fromRawData(c, f->frame.size()));
    return thedecoder->getCvImage();
}

static const int frameQueueMax = QThread::idealThreadCount() + 1;

RawVideoReader::RawVideoReader():
    stream(service), work(service), asioThread(&service)
{

}

RawVideoReader::RawVideoReader(QString filename, bool isLive):
    RawVideoReader()
{
    file.setFileName(filename);
    file.open(QIODevice::ReadOnly);
    live = file.isSequential() && isLive;
    if (!isSequential()) {
        file.seek(RawVideoSource::instance->headerBytes);
    } else {
        setupAsio(file.handle());
    }
}

RawVideoReader::RawVideoReader(FILE* processStream, bool isLive):
    RawVideoReader()
{
    live = isLive;
    process = processStream;
    setupAsio(fileno(process));
}

RawVideoReader::~RawVideoReader()
{
    // Abort reading operations before object destruction starts.
    // This also quits the background thread.
    service.stop();
    if (process)
        pclose(process);
}

VideoSourcePlugin* RawVideoReader::plugin()
{
    return RawVideoSource::instance;
}

bool RawVideoReader::isSequential()
{
    return live || process || file.isSequential();
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
        if (file.isOpen()) {
            qint64 status;
            status = file.read(f->frame.data(), s->frameBytes);
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
        if (errcode) {
            QString msg = "Error reading data: ";
            msg += QString::fromStdString(errcode.message());
            emit error(msg);
            return;
        }
        if (!live) {
            if (!frameQueue.isEmpty()) {
                bool full = frameQueue.size() == frameQueueMax;
                emit frameReady(frameQueue.dequeue());
                if (full) {
                    // Reading was stopped, start it again.
                    asioRead();
                }
            } else {
                // Fhe foreman managed to empty the queue, which means that
                // it is surely fast enough to take this frame a bit later.
                readerSlowEmitNextFrame = true;
            }
        }
    }
}

void RawVideoReader::asyncReadComplete(SharedRawFrame frame, QString err)
{
    if (!err.isNull()) {
            emit error(err);
    } else {
        frame->metaData = makeMetaData();
        if (live) {
            emit frameReady(frame);
        } else if (readerSlowEmitNextFrame) {
            readerSlowEmitNextFrame = false;
            asioRead();
            emit frameReady(frame);
        } else {
            frameQueue.enqueue(frame);
            if (frameQueue.size() < frameQueueMax)
                asioRead();
        }
    }
}

void RawVideoReader::setupAsio(int fd)
{
    RawVideoSource* s = RawVideoSource::instance;
    currentlyReadFrame = s->createRawFrame();
    auto frame = currentlyReadFrame;
    asyncHandler = [this, frame]
                   (const boost::system::error_code & err,
                    std::size_t bytes)
    {
        QString msg;
        if (err) {
            msg = "Error reading data: ";
            msg += QString::fromStdString(err.message());
        }
        QMetaObject::invokeMethod(this, "asyncReadComplete",
                                  Qt::QueuedConnection,
                                  Q_ARG(SharedRawFrame, frame->copy()),
                                  Q_ARG(QString, msg));
        if (live && msg.isEmpty())
            asioRead();
    };
    stream = decltype(stream)(service, fd);

    // Read the header.
    uint hbytes = RawVideoSource::instance->headerBytes;
    std::vector<char> bufv(hbytes);
    auto buf = boost::asio::buffer(bufv.data(), hbytes);
    try {
        boost::asio::read(stream, buf);
    } catch (boost::system::system_error err) {
        errcode = err.code();
    }
    if (!errcode) {
        stream.non_blocking(true);
        // Launch the background thread that will perform reading.
        // The 'work' object will make sure it never finishes.
        asioThread.start();
        // Get the ball rolling
        asioRead();
    }
}

void RawVideoReader::asioRead()
{
    RawVideoSource* s = RawVideoSource::instance;
    RawVideoFrame* f = static_cast<RawVideoFrame*>(currentlyReadFrame.data());
    auto buf = boost::asio::buffer(f->frame.data(), s->frameBytes);
    boost::asio::async_read(stream, buf, asyncHandler);
}

static void populateFormatSelector(QComboBox* sel)
{
    QStringList formats;
    //TODO don't use AV_PIX_FMT_NB once libav is updated
    for (int i = 0; i < AV_PIX_FMT_NB; i++) {
        if (sws_isSupportedInput((AVPixelFormat)i))
            formats.append(av_get_pix_fmt_name((AVPixelFormat)i));
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
    connect(fileName, SIGNAL(textChanged(QString)), SLOT(checkFilename(QString)));

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

    liveCheckBox = new QCheckBox("Live data, read continuously");
    layout->addRow(liveCheckBox);

    auto finishButton = new QPushButton("Finish");
    layout->addRow(finishButton);
    this->connect(finishButton, SIGNAL(clicked(bool)), SLOT(checkConfig()));
    restoreConfig();
}

void RawSourceConfigWidget::checkFilename(QString name)
{
    if (QFileInfo(name).isFile())
        liveCheckBox->setEnabled(false);
    else
        liveCheckBox->setEnabled(true);
}

void RawSourceConfigWidget::checkConfig()
{
    RawVideoSource* s = RawVideoSource::instance;
    saveConfig();
    auto result = s->initialize();
    if (s->initialize().isNull()) {
        s->saveSettings();
        emit configurationComplete();
    } else {
        QMessageBox tmp;
        tmp.setWindowTitle("Error");
        tmp.setText(result);
        tmp.exec();
        return;
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
    auto s = RawVideoSource::instance;
    s->settings.insert("file", fileName->text());
    s->settings.insert("pixformat", formatSelector->currentText());
    s->settings.insert("header_bytes", header->value());
    s->settings.insert("width", width->value());
    s->settings.insert("height", height->value());
    s->settings.insert("live", liveCheckBox->isChecked());
}

void RawSourceConfigWidget::restoreConfig()
{
    auto s = RawVideoSource::instance;
    s->readSettings();
    fileName->setText(s->settings.value("file").toString());
    int idx = formatSelector->findText(s->settings.value("pixformat").toString());
    formatSelector->setCurrentIndex(idx);
    header->setValue(s->settings.value("header_bytes").toInt());
    width->setValue(s->settings.value("width", 640).toInt());
    height->setValue(s->settings.value("height", 480).toInt());
    liveCheckBox->setChecked(s->settings.value("live", false).toBool());
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

void RawVideoSource::frameDestroyed(QByteArray frameData)
{
    framePool << frameData;
}

SharedRawFrame RawVideoSource::createRawFrame()
{
    auto frame = new RawVideoFrame;
    if (!framePool.isEmpty()) {
        frame->frame = framePool.takeLast();
    } else {
        frame->frame.resize(RawVideoSource::instance->frameBytes);
    }
    return SharedRawFrame(frame);
}

Reader* RawVideoSource::reader()
{
    return reader_.data();
}

QString RawVideoSource::settingsGroup()
{
    return "format_" + RawVideoSource::instance->name();
}

QString RawVideoSource::initialize(QString overrideInput)
{
    file = overrideInput.isNull() ? settings.value("file").toString() : overrideInput;
    size = QSize(settings.value("width", 640).toInt(),
                 settings.value("height", 480).toInt());
    pixfmt = av_get_pix_fmt(settings.value("pixformat").toString().toLatin1());
    headerBytes = settings.value("header_bytes").toInt();
    frameBytes = avpicture_get_size(pixfmt,
                                    size.width(),
                                    size.height());
    bool isLive = settings.value("live", false).toBool();

    auto& name = file;
    if (name.startsWith('<') ||
        name.startsWith('>') ||
        name.startsWith('|')) {
        // Process
        auto command = name.toLocal8Bit().data();
        std::FILE* process = popen(++command, "r");
        if (!process) {
            return "Process error: Could not launch specified command.";
        }
        reader_.reset(new RawVideoReader(process, isLive));
        return QString {};
    } else if (QFileInfo(name).isReadable()) {
        reader_.reset(new RawVideoReader(name, isLive));
        return QString {};
    } else {
        return "File error: Selected file is not readable.";
    }
}

Q_IMPORT_PLUGIN(RawVideo)
