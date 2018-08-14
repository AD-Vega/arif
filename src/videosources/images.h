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

#ifndef IMAGES_H
#define IMAGES_H

#define QT_STATICPLUGIN

#include "videosources/interfaces.h"
#include <qarvdecoder.h>
#include <QLineEdit>
#include <QRadioButton>
#include <QMetaType>

namespace Images
{

class ImageReader;

class ImageSource: public QObject, public VideoSourcePlugin
{
    Q_OBJECT
    Q_INTERFACES(VideoSourcePlugin)
    Q_PLUGIN_METADATA(IID "si.ad-vega.arif.ImageSource")

public:
    explicit ImageSource(QObject* parent = 0);
    QString name();
    QString readableName();
    VideoSourceConfigurationWidget* createConfigurationWidget();
    SharedRawFrame createRawFrame();
    SharedDecoder createDecoder();
    Reader* reader();
    QString settingsGroup();
    QString initialize(QString overrideInput = QString{});

    static ImageSource* instance;

public:
    QSize size;
    QScopedPointer<ImageReader> reader_;
    enum AVPixelFormat pixfmt;
    QStringList selectedFiles;

    friend class ImageConfigWidget;
    friend class ImageFrame;
    friend class ImageReader;
    friend class ImageDecoder;
};

class ImageConfigWidget : public VideoSourceConfigurationWidget {
  Q_OBJECT

public:
  ImageConfigWidget();

public slots:
  void getFiles();
  void getDirectory();
  void checkConfig();

private slots:
  void invalidateSelection();
  void saveConfig();
  void restoreConfig();

private:
  QRadioButton* fileRadio;
  QRadioButton* directoryRadio;
  QLineEdit* fileName;
  QLineEdit* directoryName;
};

class ImageFrame: public RawFrame
{
public:
    SharedRawFrame copy();
    VideoSourcePlugin* plugin();
    void serialize(QDataStream& s);
    void load(QDataStream& s);

private:
    QString filename;
    friend class ImageDecoder;
    friend class ImageSource;
    friend class ImageReader;
};

class ImageDecoder: public Decoder
{
public:
    const cv::Mat decode(RawFrame* in);
    VideoSourcePlugin* plugin();
};

class ImageReader: public Reader
{
    Q_OBJECT

public:
    ImageReader(QStringList files);
    bool seek(qint64 frame);
    bool isSequential();
    quint64 numberOfFrames();
    VideoSourcePlugin* plugin();

public slots:
    void readFrame();

private:
    QStringList filenames;
    quint64 current = 0;
    friend class ImageSource;
};

}

#endif
