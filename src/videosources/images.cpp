/*
    arif, ADV Realtime Image Filtering, a tool for amateur astronomy.
    Copyright (C) 2014 Jure Varlec <jure.varlec@ad-vega.si>
                       Andrej Lajovic <andrej.lajovic@ad-vega.si>

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

#include "videosources/images.h"
#include <opencv2/highgui/highgui.hpp>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>

using namespace Images;

SharedRawFrame ImageFrame::copy()
{
    auto F = ImageSource::instance->createRawFrame();
    auto f = static_cast<ImageFrame*>(F.data());
    f->filename = filename;
    return F;
}

void ImageFrame::load(QDataStream& s)
{
    s >> filename;
    RawFrame::load(s);
}

VideoSourcePlugin* ImageFrame::plugin()
{
    return ImageSource::instance;
}

void ImageFrame::serialize(QDataStream& s)
{
    s << filename;
    RawFrame::serialize(s);
}

const cv::Mat ImageDecoder::decode(RawFrame* in)
{
    auto f = static_cast<ImageFrame*>(in);
    return cv::imread(f->filename.toStdString(),
                      CV_LOAD_IMAGE_ANYCOLOR | CV_LOAD_IMAGE_ANYDEPTH);
}

VideoSourcePlugin* ImageDecoder::plugin()
{
    return ImageSource::instance;
}

ImageReader::ImageReader(QStringList files): filenames(files)
{

}

bool ImageReader::isSequential()
{
    return false;
}

quint64 ImageReader::numberOfFrames()
{
    return filenames.size();
}

bool ImageReader::seek(qint64 frame)
{
    if (frame < 0 || frame >= filenames.size())
        return false;
    current = frame;
    return true;
}

VideoSourcePlugin* ImageReader::plugin()
{
    return ImageSource::instance;
}

void ImageReader::readFrame()
{
    if (current >= (quint64)(filenames.size())) {
        emit atEnd();
    } else {
        auto F = ImageSource::instance->createRawFrame();
        auto f = static_cast<ImageFrame*>(F.data());
        f->metaData = makeMetaData();
        f->filename = filenames.at(current++);
        emit frameReady(F);
    }
}

ImageSource* ImageSource::instance;

ImageSource::ImageSource(QObject* parent): QObject(parent)
{
    ImageSource::instance = this;
}

QString ImageSource::name()
{
    return "Images";
}

QString ImageSource::readableName()
{
    return "Image files";
}

QString ImageSource::settingsGroup()
{
    return "format_" + ImageSource::instance->name();
}

Reader* ImageSource::reader()
{
    return reader_.data();
}

SharedRawFrame ImageSource::createRawFrame()
{
    return SharedRawFrame(new ImageFrame);
}

SharedDecoder ImageSource::createDecoder()
{
    return SharedDecoder(new ImageDecoder);
}

VideoSourceConfigurationWidget* ImageSource::createConfigurationWidget()
{
    return new ImageConfigWidget;
}

ImageConfigWidget::ImageConfigWidget():
    VideoSourceConfigurationWidget("Image list configuration")
{

    auto layout = new QVBoxLayout(this);
    fileRadio = new QRadioButton("Image index or multiple image files");
    layout->addWidget(fileRadio);
    auto fileInputRow = new QHBoxLayout();
    fileName = new QLineEdit();
    fileInputRow->addWidget(fileName);
    auto openFileButton = new QPushButton("Open");
    fileInputRow->addWidget(openFileButton);
    layout->addLayout(fileInputRow);
    connect(fileRadio, SIGNAL(toggled(bool)), fileName, SLOT(setEnabled(bool)));
    connect(fileRadio, SIGNAL(toggled(bool)), openFileButton, SLOT(setEnabled(bool)));
    this->connect(openFileButton, SIGNAL(clicked(bool)), SLOT(getFiles()));
    fileRadio->setChecked(true);

    directoryRadio = new QRadioButton("Image directory");
    layout->addWidget(directoryRadio);
    auto directoryInputRow = new QHBoxLayout();
    directoryName = new QLineEdit();
    directoryInputRow->addWidget(directoryName);
    auto openDirectoryButton = new QPushButton("Open");
    directoryInputRow->addWidget(openDirectoryButton);
    layout->addLayout(directoryInputRow);
    connect(directoryRadio, SIGNAL(toggled(bool)), directoryName, SLOT(setEnabled(bool)));
    connect(directoryRadio, SIGNAL(toggled(bool)), openDirectoryButton, SLOT(setEnabled(bool)));
    this->connect(openDirectoryButton, SIGNAL(clicked(bool)), SLOT(getDirectory()));
    directoryRadio->setChecked(true);

    auto iconClear = QIcon::fromTheme("document-open");
    if (!iconClear.isNull()) {
        openFileButton->setText("");
        openFileButton->setIcon(iconClear);
        openDirectoryButton->setText("");
        openDirectoryButton->setIcon(iconClear);
    }

    auto finishButton = new QPushButton("Finish");
    layout->addWidget(finishButton);
    this->connect(finishButton, SIGNAL(clicked(bool)), SLOT(checkConfig()));
    this->connect(this, SIGNAL(configurationComplete()), SLOT(saveConfig()));
    restoreConfig();
}

void ImageConfigWidget::getFiles()
{
    auto s = ImageSource::instance;
    s->selectedFiles = QFileDialog::getOpenFileNames();
    if (s->selectedFiles.size() == 1)
        fileName->setText(s->selectedFiles.at(0));
    else if (s->selectedFiles.size() > 1) {
        fileName->setPlaceholderText("(" +
                                     QString::number(s->selectedFiles.size()) +
                                     " files)");
        fileName->clear();
        connect(fileName, SIGNAL(textChanged(QString)),
                this, SLOT(invalidateSelection()), Qt::UniqueConnection);
    }
}

void ImageConfigWidget::invalidateSelection()
{
    auto s = ImageSource::instance;
    s->selectedFiles = QStringList();
    fileName->setPlaceholderText(QString());
    disconnect(fileName, SIGNAL(textChanged(QString)),
               this, SLOT(invalidateSelection()));
}

void ImageConfigWidget::getDirectory()
{
    auto selected = QFileDialog::getExistingDirectory();
    if (!selected.isNull())
        directoryName->setText(selected);
}

void ImageConfigWidget::checkConfig()
{
    saveConfig();
    auto s = ImageSource::instance;
    auto status = s->initialize();
    if (!status.isEmpty()) {
        QMessageBox::critical(this, "Image source error", status);
    } else {
        s->saveSettings();
        emit(configurationComplete());
    }
}

static QStringList loadFilesFromIndex(QString indexfile)
{
    QFile imagelist(indexfile);
    imagelist.open(QIODevice::ReadOnly);
    QTextStream s(&imagelist);
    QDir dir(QFileInfo(imagelist).absoluteDir());
    QStringList files;
    while (!s.atEnd())
        files << dir.absoluteFilePath(s.readLine());
    imagelist.close();
    return files;
}

static QStringList loadFilesFromDirectory(QString directory)
{
    QDir dir(directory);
    QStringList files(dir.entryList(QDir::Files, QDir::Name));
    QStringList absFiles;
    for (auto f = files.begin(); f != files.end(); f++)
        absFiles << dir.absoluteFilePath(*f);
    return absFiles;
}

static bool queryFrameSize(QString file)
{
    try {
        auto testimage = cv::imread(file.toStdString());
        ImageSource::instance->size = QSize(testimage.cols, testimage.rows);
        return true;
    } catch (std::exception& e) {
        return false;
    }
}

void ImageConfigWidget::saveConfig()
{
    auto s = ImageSource::instance;
    if (fileRadio->isChecked())
        s->settings.insert("type", "file");
    else
        s->settings.insert("type", "directory");

    s->settings.insert("file", fileName->text());
    s->settings.insert("directory", directoryName->text());
}

void ImageConfigWidget::restoreConfig()
{
    auto s = ImageSource::instance;
    s->readSettings();
    fileName->setText(s->settings.value("file").toString());
    directoryName->setText(s->settings.value("directory").toString());
    auto type = s->settings.value("type", "file").toString();
    if (type == "directory")
        directoryRadio->setChecked(true);
    else
        fileRadio->setChecked(true);
}

QString ImageSource::initialize(QString overrideInput)
{
    QStringList files;
    bool isFile = "file" == settings.value("type", "file").toString();
    if (isFile) {
        if (!overrideInput.isEmpty()) {
            return "Images can only be given as a directory when in batch mode.";
        }
        auto filename = settings.value("file").toString();
        if (filename.isEmpty()) {
            if (selectedFiles.size() > 0)
                files = selectedFiles;
        } else {
            if (QFileInfo(filename).isReadable())
                files = loadFilesFromIndex(filename);
            else {
                return "File error: selected file is not readable.";
            }
        }
    } else {
        auto dirname = settings.value("directory").toString();
        if (QFileInfo(dirname).isDir())
            files = loadFilesFromDirectory(dirname);
        else {
            return "Directory error: selected directory is not valid.";
        }
    }
    if ((files.size() > 0) && queryFrameSize(files.first())) {
        reader_.reset(new ImageReader(files));
        return QString{};
    }
    return "No images to load.";
}

Q_EXPORT_PLUGIN2(Images, Images::ImageSource)
Q_IMPORT_PLUGIN(Images)
