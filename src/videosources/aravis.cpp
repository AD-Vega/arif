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

#include "videosources/aravis.h"
#include <qarvgui.h>
#include <QVBoxLayout>
#include <QLayout>

using namespace Aravis;

SharedRawFrame AravisFrame::copy()
{
    auto f = new AravisFrame;
    *f = *this;
    return SharedRawFrame(f);
}

void AravisFrame::load(QDataStream& s)
{
    s >> frame;
    RawFrame::load(s);
}

void AravisFrame::serialize(QDataStream& s)
{
    s << frame;
    RawFrame::serialize(s);
}

VideoSourcePlugin* AravisFrame::plugin()
{
    return AravisSource::instance;
}

AravisDecoder::AravisDecoder(ArvPixelFormat pixfmt, QSize size):
    thedecoder(QArvDecoder::makeDecoder(pixfmt, size, true))
{

}

const cv::Mat AravisDecoder::decode(RawFrame* in)
{
    auto f = static_cast<AravisFrame*>(in);
    thedecoder->decode(f->frame);
    return thedecoder->getCvImage();
}

VideoSourcePlugin* AravisDecoder::plugin()
{
    return AravisSource::instance;
}

bool AravisReader::isSequential()
{
    return true;
}

quint64 AravisReader::numberOfFrames()
{
    return 0;
}

VideoSourcePlugin* AravisReader::plugin()
{
    return AravisSource::instance;
}

void AravisReader::readFrame()
{

}

bool AravisReader::seek(qint64 frame)
{
    return false;
}

void AravisReader::getFrame()
{
    auto f = new AravisFrame;
    f->frame = camera->getFrame(true, true);
    emit frameReady(SharedRawFrame(f));
}

AravisSource* AravisSource::instance;

AravisSource::AravisSource(QObject* parent):
    QObject(parent), reader_(new AravisReader)
{
    AravisSource::instance = this;
    auto a = dynamic_cast<QApplication*>(QApplication::instance());
    Q_CHECK_PTR(a);
    QArvCamera::init();
    QArvGui::init(a);
}

QString AravisSource::name()
{
    return "Aravis";
}

QString AravisSource::readableName()
{
    return "Ethernet camera";
}

Reader* AravisSource::reader()
{
    return reader_.data();
}

SharedDecoder AravisSource::createDecoder()
{
    return SharedDecoder(new AravisDecoder(pixfmt, size));
}

SharedRawFrame AravisSource::createRawFrame()
{
    return SharedRawFrame(new AravisFrame);
}

VideoSourceConfigurationWidget*
AravisSource::createConfigurationWidget()
{
    return new AravisSourceConfigWidget();
}

AravisSourceConfigWidget::AravisSourceConfigWidget(QWidget* parent):
    VideoSourceConfigurationWidget("Ethernet camera configuration", parent),
    gui(new QArvGui(this, false))
{
    setLayout(new QVBoxLayout);
    layout()->addWidget(gui);
    gui->show();
    connect(gui, SIGNAL(recordingToggled(bool)), SLOT(finish()));
}

void AravisSourceConfigWidget::finish()
{
    auto s = AravisSource::instance;
    auto c = gui->camera();
    s->size = c->getFrameSize();
    s->pixfmt = c->getPixelFormatId();
    disconnect(gui, SIGNAL(recordingToggled(bool)), this, SLOT(finish()));
    gui->forceRecording();
    s->reader_->camera = c;
    connect(c, SIGNAL(frameReady()), s->reader_.data(), SLOT(getFrame()));
    gui->setParent(0);
    emit configurationComplete(gui);
}

Q_EXPORT_PLUGIN2(Aravis, Aravis::AravisSource)
Q_IMPORT_PLUGIN(Aravis)
