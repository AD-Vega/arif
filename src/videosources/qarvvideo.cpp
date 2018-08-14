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

#include "videosources/qarvvideo.h"
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

using namespace QArvVideo;

QArvVideoSource* QArvVideoSource::instance;

QArvVideoSource::QArvVideoSource(QObject* parent): QObject(parent)
{
    instance = this;
}

QArvVideoFrame::~QArvVideoFrame()
{
    QArvVideoSource::instance->frameDestroyed(frame);
}

SharedRawFrame QArvVideoFrame::copy()
{
    SharedRawFrame f = QArvVideoSource::instance->createRawFrame();
    QArvVideoFrame* r = static_cast<QArvVideoFrame*>(f.data());
    *r = *this;
    return f;
}

VideoSourcePlugin* QArvVideoFrame::plugin()
{
    return QArvVideoSource::instance;
}

void QArvVideoFrame::serialize(QDataStream& s)
{
    s.writeRawData(frame.constData(), frame.size());
    RawFrame::serialize(s);
}

void QArvVideoFrame::load(QDataStream& s)
{
    frame.resize(QArvVideoSource::instance->frameBytes);
    s.readRawData(frame.data(), QArvVideoSource::instance->frameBytes);
    RawFrame::load(s);
}

QArvVideoDecoder::QArvVideoDecoder()
{
    auto s = QArvVideoSource::instance;
    auto d = s->reader_->qarvVideo.makeDecoder();
    thedecoder.reset(d);
}

VideoSourcePlugin* QArvVideoDecoder::plugin()
{
    return QArvVideoSource::instance;
}

const cv::Mat QArvVideoDecoder::decode(RawFrame* in)
{
    auto f = static_cast<QArvVideoFrame*>(in);
    auto c = f->frame.constData();
    thedecoder->decode(QByteArray::fromRawData(c, f->frame.size()));
    return thedecoder->getCvImage();
}

QArvVideoReader::QArvVideoReader(QString filename):
    qarvVideo(filename)
{
    if (qarvVideo.status() == false)
        emit error("Could not read video description file.");
}

VideoSourcePlugin* QArvVideoReader::plugin()
{
    return QArvVideoSource::instance;
}

bool QArvVideoReader::isSequential()
{
    return !qarvVideo.isSeekable();
}

quint64 QArvVideoReader::numberOfFrames()
{
    if (isSequential())
        return 0;
    return qarvVideo.numberOfFrames();
}

bool QArvVideoReader::seek(qint64 frame)
{
    if (isSequential())
        return false;
    return qarvVideo.seek(frame);
}

void QArvVideoReader::readFrame()
{
    QArvVideoSource* s = QArvVideoSource::instance;
    SharedRawFrame frm = s->createRawFrame();
    QArvVideoFrame* f = static_cast<QArvVideoFrame*>(frm.data());
    f->frame = qarvVideo.read();
    if (f->frame.isNull()) {
        if (qarvVideo.atEnd()) {
            emit atEnd();
        } else if (qarvVideo.error() == QFile::NoError &&
                   qarvVideo.status() == false) {
            emit error("Could not read video description file.");
        } else {
            emit error(qarvVideo.errorString());
        }
    } else {
        frm->metaData = makeMetaData();
        emit frameReady(frm);
    }
}

QArvSourceConfigWidget::QArvSourceConfigWidget() :
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

    auto finishButton = new QPushButton("Finish");
    layout->addRow(finishButton);
    this->connect(finishButton, SIGNAL(clicked(bool)), SLOT(checkConfig()));
    restoreConfig();
}

void QArvSourceConfigWidget::checkConfig()
{
    saveConfig();
    auto error = QArvVideoSource::instance->initialize();
    if (!error.isEmpty()) {
        QMessageBox tmp;
        tmp.setWindowTitle("File error");
        tmp.setText(error);
        tmp.exec();
    } else {
        QArvVideoSource::instance->saveSettings();
        emit configurationComplete();
    }
}

void QArvSourceConfigWidget::getFile()
{
    auto tmp = QFileDialog::getOpenFileName(this,
                                            "Open QArv video file",
                                            fileName->text());
    if (!tmp.isNull())
        fileName->setText(tmp);
}

void QArvSourceConfigWidget::saveConfig()
{
    auto s = QArvVideoSource::instance;
    s->settings.insert("file", fileName->text());
}

void QArvSourceConfigWidget::restoreConfig()
{
    auto s = QArvVideoSource::instance;
    s->readSettings();
    fileName->setText(s->settings.value("file").toString());
}

QString QArvVideoSource::name()
{
    return "QArvRecordedVideo";
}

QString QArvVideoSource::readableName()
{
    return "QArv video file";
}

QString QArvVideoSource::settingsGroup()
{
    return "format_" + QArvVideoSource::instance->name();
}

VideoSourceConfigurationWidget* QArvVideoSource::createConfigurationWidget()
{
    return new QArvSourceConfigWidget;
}

SharedDecoder QArvVideoSource::createDecoder()
{
    return SharedDecoder(new QArvVideoDecoder);
}

void QArvVideoSource::frameDestroyed(QByteArray frameData)
{
    framePool << frameData;
}

SharedRawFrame QArvVideoSource::createRawFrame()
{
    auto frame = new QArvVideoFrame;
    if (!framePool.isEmpty()) {
        frame->frame = framePool.takeLast();
    } else {
        frame->frame.resize(QArvVideoSource::instance->frameBytes);
    }
    return SharedRawFrame(frame);
}

Reader* QArvVideoSource::reader()
{
    return reader_.data();
}

QString QArvVideoSource::initialize(QString overrideInput)
{
    auto name = overrideInput.isEmpty() ? settings.value("file").toString() : overrideInput;
    if (QFileInfo(name).isReadable()) {
        reader_.reset(new QArvVideoReader(name));
        auto& qv = reader_->qarvVideo;
        if (!qv.status()) {
            return "File error: selected file is not readable.";
        }
        size = qv.frameSize();
        frameBytes = qv.frameBytes();
        return QString{};
    }
    return "Error opening video.";
}

Q_IMPORT_PLUGIN(QArvVideoSource)
