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

#include "processing.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <QFont>
#include <QFontMetrics>
#include <cstdint>

void DecodeStage(SharedData d);
void CropStage(SharedData d);
void EstimateQualityStage(SharedData d);
void RenderStage(SharedData d);
void SaveStage(SharedData d);

static void (*stages[])(SharedData) = {
    DecodeStage,
    RenderStage,
    CropStage,
    EstimateQualityStage,
    SaveStage,
};

SharedData processData(SharedData data)
{
    if (data->onlyRender) {
        DecodeStage(data);
        if (data->stageSuccessful)
            RenderStage(data);
    } else {
        for (int i = 0; i < sizeof(stages) / sizeof(void*); i++) {
            stages[i](data);
            if (!data->stageSuccessful)
                return data;
        }
    }
    return data;
}

void DecodeStage(SharedData d)
{
    d->completedStages << "Decode";
    d->decoded = d->decoder->decode(d->rawFrame.data());
    if (d->settings->negative) {
        double maxval;
        switch (d->decoded.depth()) {
            case CV_8U:
                cv::subtract(UINT8_MAX, d->decoded, d->decoded);
                break;
            case CV_8S:
                cv::subtract(INT8_MAX, d->decoded, d->decoded);
                break;
            case CV_16U:
                cv::subtract(UINT16_MAX, d->decoded, d->decoded);
                break;
            case CV_16S:
                cv::subtract(INT16_MAX, d->decoded, d->decoded);
                break;
            case CV_32S:
                cv::subtract(INT32_MAX, d->decoded, d->decoded);
                break;
            default:
                cv::minMaxIdx(d->decoded.reshape(1), nullptr, &maxval);
                cv::subtract(maxval, d->decoded, d->decoded);
        }
    }
    if (d->decoded.type() != CV_32F)
        d->decoded.convertTo(d->decodedFloat, CV_32F);
    else
        d->decodedFloat = d->decoded;
    if (d->decodedFloat.channels() > 1)
        cv::cvtColor(d->decodedFloat, d->grayscale, CV_BGR2GRAY);
    else
        d->grayscale = d->decodedFloat;
    d->stageSuccessful = true;
    d->errorMessage.clear();
}

void CropStage(SharedData d)
{
    d->completedStages << "Crop";

    const cv::Mat& m = d->grayscale;
    QRect imageRect(0, 0, m.cols, m.rows);
    if (!d->settings->doCrop) {
        d->cropArea = imageRect;
        d->cvCropArea = cv::Rect(imageRect.x(), imageRect.y(),
                                 imageRect.width(), imageRect.height());
        d->stageSuccessful = true;
        d->errorMessage.clear();
        return;
    }

    float black = d->settings->threshold;
    float sum = 0, x = 0, y = 0;
    for (int i = 0; i < m.rows; i++) {
        const float* row = m.ptr<float>(i);
        for (int j = 0; j < m.cols; j++) {
            const float val = row[j] > black ? 1.0 : 0.0;
            sum += val;
            x += val * j;
            y += val * i;
        }
    }
    x /= sum;
    y /= sum;

    QRect cropRect(0, 0,
                   d->settings->cropWidth,
                   d->settings->cropWidth);
    cropRect.moveCenter(QPoint(x, y));
    d->cropArea = cropRect;
    d->cvCropArea = cv::Rect(cropRect.x(), cropRect.y(),
                             cropRect.width(), cropRect.height());

    if (!imageRect.contains(cropRect)) {
        d->stageSuccessful = false;
        d->errorMessage = "Crop rectangle out of image bounds";
        if (d->doRender) {
            static const QPainterPath message = []{
                QFont font;
                font.setPixelSize(20);
                QFontMetrics metrics(font);
                QPainterPath path;
                path.addText(-metrics.minLeftBearing() + 10,
                             -metrics.descent() - 10,
                             font,
                             "Out of bounds!");
                return path;
            }();
            PaintObject po1;
            po1.pen.setColor(Qt::red);
            po1.pen.setWidth(5);
            po1.path.addRect(d->renderedFrame.rect());
            PaintObject po2;
            po2.pen.setColor(Qt::red);
            po2.brush.setColor(Qt::red);
            po2.brush.setStyle(Qt::SolidPattern);
            po2.path = message;
            po2.path.translate(0, d->renderedFrame.height());
            d->paintObjects << po1 << po2;
        }
    } else {
        if (d->doRender) {
            PaintObject po1;
            po1.pen.setColor(Qt::black);
            po1.pen.setWidth(0);
            po1.path.addRect(cropRect);
            PaintObject po2;
            po2.pen.setColor(Qt::white);
            po2.pen.setWidth(0);
            po2.pen.setStyle(Qt::DotLine);
            po2.path.addRect(cropRect);
            d->paintObjects << po1 << po2;
        }
        d->stageSuccessful = true;
        d->errorMessage.clear();
    }
}

// Imported from QArvMainWindow
template<bool grayscale, bool depth8>
void renderFrame(const cv::Mat frame, QImage* image_, bool markClipped = false,
                 Histograms* hists = NULL, bool logarithmic = false) {
  typedef typename ::std::conditional<depth8, uint8_t, uint16_t>::type ImageType;
  float * histRed, * histGreen, * histBlue;
  if (hists) {
    histRed = hists->red;
    histGreen = hists->green;
    histBlue = hists->blue;
    for (int i = 0; i < 256; i++) {
      histRed[i] = histGreen[i] = histBlue[i] = 0;
    }
  } else {
    histRed = histGreen = histBlue = NULL;
  }
  QImage& image = *image_;
  const int h = frame.rows, w = frame.cols;
  QSize s = image.size();
  if (s.height() != h
      || s.width() != w
      || image.format() != QImage::Format_ARGB32_Premultiplied)
    image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
  if (!grayscale) {
    float* histograms[3] = { histRed, histGreen, histBlue };
    for (int i = 0; i < h; i++) {
      auto imgLine = image.scanLine(i);
      auto imageLine = frame.ptr<cv::Vec<ImageType, 3> >(i);
      for (int j = 0; j < w; j++) {
        auto& bgr = imageLine[j];
        bool clipped = false;
        for (int px = 0; px < 3; px++) {
          uint8_t tmp = depth8 ? bgr[2-px] : bgr[2-px] >> 8;
          if (hists)
            histograms[px][tmp]++;
          clipped = clipped || tmp == 255;
          imgLine[4*j + 2 - px] = tmp;
        }
        imgLine[4*j + 3] = 255;
        if (clipped && markClipped) {
          imgLine[4*j + 0] = 255;
          imgLine[4*j + 1] = 0;
          imgLine[4*j + 2] = 200;
        }
      }
    }
    if (hists && logarithmic) {;
      for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++) {
          float* h = histograms[c] + i;
          *h = log2(*h + 1);
        }
    }
  } else {
    for (int i = 0; i < h; i++) {
      auto imgLine = image.scanLine(i);
      auto imageLine = frame.ptr<ImageType>(i);
      for (int j = 0; j < w; j++) {
        uint8_t gray = depth8 ? imageLine[j] : imageLine[j] >> 8;
        if (hists)
          histRed[gray]++;
        if (gray == 255 && markClipped) {
          imgLine[4*j + 0] = 255;
          imgLine[4*j + 1] = 0;
          imgLine[4*j + 2] = 200;
        } else {
          for (int px = 0; px < 3; px++) {
            imgLine[4*j + px] = gray;
          }
        }
        imgLine[4*j + 3] = 255;
      }
    }
    if (hists && logarithmic)
      for (int i = 0; i < 256; i++)
        histRed[i] = log2(histRed[i] + 1);
  }
}


void RenderStage(SharedData d)
{
    if (!d->doRender) {
        d->stageSuccessful = true;
        d->errorMessage.clear();
        return;
    }
    d->completedStages << "Render";
    void (*theFunc) (const cv::Mat, QImage*, bool, Histograms*, bool);
    cv::Mat* M = &d->decoded;
    switch (M->type()) {
    case CV_16UC1:
        theFunc = renderFrame<true, false>;
        break;
    case CV_16UC3:
        theFunc = renderFrame<false, false>;
        break;
    case CV_8UC1:
        theFunc = renderFrame<true, true>;
        break;
    case CV_8UC3:
        theFunc = renderFrame<false, true>;
        break;
    default:
        M->convertTo(d->renderTemporary, CV_8U);
        M = &d->renderTemporary;
        theFunc = M->channels() > 1 ?
                  renderFrame<false, true> :
                  renderFrame<true, true>;
    }
    theFunc(*M, &d->renderedFrame, d->settings->markClipped,
            d->histograms.data(), d->settings->logarithmicHistograms);
    d->stageSuccessful = true;
    d->errorMessage.clear();
}

void EstimateQualityStage(SharedData d)
{
    d->completedStages << "EstimateQuality";
    cv::GaussianBlur(d->decodedFloat, d->blurNoise,
                     cv::Size(0, 0), d->settings->noiseSigma);
    cv::GaussianBlur(d->blurNoise, d->blurSignal,
                     cv::Size(0, 0), d->settings->signalSigma);
    cv::subtract(d->blurNoise, d->blurSignal, d->blurSignal);
    cv::subtract(d->decodedFloat, d->blurNoise, d->blurNoise);
    double noise = d->blurNoise.dot(d->blurNoise);
    if (noise == 0) {
        d->quality = 0;
    } else {
        double signal = d->blurSignal.dot(d->blurSignal);
        d->quality = signal / noise;
    }
    d->stageSuccessful = true;
    d->errorMessage.clear();
}

void SaveStage(SharedData d)
{
    d->completedStages << "Save";
    d->stageSuccessful = true;
    d->errorMessage.clear();

    auto& meta = d->rawFrame->metaData;
    QString fnTemplate("%1/frame-%2-%3-q%4");
    QString filename = fnTemplate
                       .arg(d->settings->saveImagesDirectory)
                       .arg(meta.timestamp.toString("yyyyMMdd-hhmmsszzz"))
                       .arg(meta.frameOfSecond, 3, 10, QChar('0'))
                       .arg(d->quality, 0, 'g', 4);
    d->filename = filename;

    if (d->settings->saveImages &&
        d->settings->filterType == QualityFilterType::AcceptanceRate) {
        d->decoded(d->cvCropArea).copyTo(*(d->cloned));
    }

    d->accepted = d->quality >= d->settings->minimumQuality;
    bool doSave = d->settings->filterType == QualityFilterType::None ||
                  (d->settings->filterType == QualityFilterType::MinimumQuality && d->accepted);
    doSave = doSave && d->settings->saveImages;
    if (doSave) {
        d->stageSuccessful = saveImage(d->decoded(d->cvCropArea), filename);
        if (!d->stageSuccessful)
            d->errorMessage = "filename " + filename;
    }
}

bool saveImage(const cv::Mat& image, QString filename)
{
    return cv::imwrite((filename + ".tiff").toStdString(), image);
}
