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

#include "plotwidgets.h"
#include <QStyleOption>
#include <QDebug>

static const Qt::GlobalColor color1 = Qt::green;
static const Qt::GlobalColor color2 = Qt::red;

static const int N = 30, n = 100;

static QCPItemText* createSamplingLabel(QCustomPlot* parent)
{
    static QFont font = []() {
        QFont font;
        font.setPixelSize(20);
        font.setBold(true);
        return font;
    }();
    QStyleOption style;
    auto samplingLabel = new QCPItemText(parent);
    samplingLabel->setFont(font);
    samplingLabel->setText("Not enough frames yet");
    samplingLabel->setColor(style.palette.color(QPalette::Text));
    samplingLabel->setTextAlignment(Qt::AlignCenter);
    samplingLabel->setPositionAlignment(Qt::AlignCenter);
    samplingLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    samplingLabel->position->setCoords(0.5, 0.5);
    return samplingLabel;
}

QualityGraph::QualityGraph(QWidget* parent):
    QCustomPlot(parent),
    longGraphMean(boost::accumulators::tag::rolling_window::window_size = n)
{
    QStyleOption style;
    shortGraph = addGraph(xAxis2, yAxis);
    longGraph = addGraph(xAxis, yAxis);

    xAxis->setLabel("Frame number (all frames)");
    xAxis->setLabelColor(color1);
    xAxis->setTickLabelColor(color1);
    xAxis->setTickPen(QPen(color1));
    xAxis->setBasePen(QPen(color1));

    xAxis2->setLabel(QString("Frame number (last %1 frames)").arg(shortLength));
    xAxis2->setVisible(true);
    xAxis2->setTickPen(QPen(color2));
    xAxis2->setLabelColor(color2);
    xAxis2->setTickLabelColor(color2);
    xAxis2->setTickPen(QPen(color2));
    xAxis2->setBasePen(QPen(color2));

    yAxis->setLabel("Quality");
    yAxis->setLabelColor(style.palette.color(QPalette::Text));
    yAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    yAxis->setTickPen(style.palette.color(QPalette::Text));
    yAxis->setBasePen(style.palette.color(QPalette::Text));

    longGraph->setPen(QPen(color1));
    shortGraph->setPen(QPen(color2));
    setBackground(style.palette.background());

    showSamplingText();

    setNotAntialiasedElements(QCP::aeAll);
    setPlottingHints(QCP::phFastPolylines | QCP::phCacheLabels);
}

void QualityGraph::setShortGraphMaxFrames(int frames)
{
    shortLength = frames;
    if (shortGraph->data()->size() > shortLength) {
        shortGraph->removeDataBefore(counter - shortLength);
    }
    xAxis2->setLabel(QString("Frame number (last %1 frames)").arg(shortLength));
    replot();
}

void QualityGraph::addFrameStats(SharedData data)
{
    if (data->stageSuccessful
            && data->completedStages.contains(ProcessingStage::EstimateQuality)) {
        ++counter;
        longGraphMean(data->quality);
        longGraph->addData(counter,
                           boost::accumulators::rolling_mean(longGraphMean));
        shortGraph->addData(counter, data->quality);
        if (shortGraph->data()->size() > shortLength) {
            shortGraph->removeDataBefore(counter - shortLength);
        }
    }
}

void QualityGraph::clear()
{
    counter = 0;
    longGraph->clearData();
    shortGraph->clearData();
    showSamplingText();
    replot();
}

void QualityGraph::draw()
{
    if (samplingLabel) {
        removeItem(samplingLabel);
        samplingLabel = nullptr;
    }
    rescaleAxes();
    replot();
}

void QualityGraph::showSamplingText()
{
    samplingLabel = createSamplingLabel(this);
    addItem(samplingLabel);
}

QualityHistogram::QualityHistogram(QWidget* parent):
    QCustomPlot(parent),
    accumulator(boost::accumulators::tag::density::cache_size = n,
                boost::accumulators::tag::density::num_bins = N)
{
    QStyleOption style;
    graph = new QCPBars(xAxis, yAxis);
    addPlottable(graph);

    xAxis->setLabel("Quality of frames");
    xAxis->setLabelColor(style.palette.color(QPalette::Text));
    xAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    xAxis->setTickPen(QPen(style.palette.color(QPalette::Text)));
    xAxis->setBasePen(QPen(style.palette.color(QPalette::Text)));

    yAxis->setLabel("Percentage of frames");
    yAxis->setLabelColor(style.palette.color(QPalette::Text));
    yAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    yAxis->setTickPen(style.palette.color(QPalette::Text));
    yAxis->setBasePen(style.palette.color(QPalette::Text));

    QColor tmp = style.palette.color(QPalette::Text);
    tmp.setAlpha(64);
    graph->setPen(QPen(style.palette.color(QPalette::Text)));
    graph->setBrush(tmp);
    setBackground(style.palette.background());

    showSamplingText();

    setNotAntialiasedElements(QCP::aeAll);
    setPlottingHints(QCP::phFastPolylines | QCP::phCacheLabels);
}

void QualityHistogram::addFrameStats(SharedData data)
{
    if (data->stageSuccessful
            && data->completedStages.contains(ProcessingStage::EstimateQuality)) {
        ++counter;
        accumulator(data->quality);
    }
}

void QualityHistogram::clear()
{
    accumulator = decltype(accumulator)(boost::accumulators::tag::density::cache_size = n,
                                        boost::accumulators::tag::density::num_bins = N);
    counter = 0;
    graph->clearData();
    showSamplingText();
    replot();
}

void QualityHistogram::draw()
{
    if (counter > n) {
        auto histogram = boost::accumulators::density(accumulator);
        if (samplingLabel) {
            removeItem(samplingLabel);
            samplingLabel = nullptr;
            // Set bar width.
            auto v2 = histogram.begin();
            auto v1 = v2;
            ++v1;
            graph->setWidth((v2->first - v1->first));
        }
        graph->clearData();
        for (auto& h : histogram) {
            graph->addData(h.first, h.second*100);
        }
        rescaleAxes();
        replot();
    }
}

void QualityHistogram::showSamplingText()
{
    samplingLabel = createSamplingLabel(this);
    addItem(samplingLabel);
}

ImageHistogram::ImageHistogram(QWidget* parent): QCustomPlot(parent)
{
    QStyleOption style;
    red = new QCPBars(xAxis, yAxis);
    green = new QCPBars(xAxis, yAxis);
    blue = new QCPBars(xAxis, yAxis);
    addPlottable(red);
    addPlottable(green);
    addPlottable(blue);
    green->moveAbove(blue);
    red->moveAbove(green);

    xAxis->setTicks(false);
    xAxis->setTickLabels(false);
    xAxis->setBasePen(style.palette.color(QPalette::Text));

    yAxis->setTicks(false);
    yAxis->setTickLabels(false);
    yAxis->setBasePen(style.palette.color(QPalette::Text));

    red->setPen(QPen(QColor(255, 0, 0, 128)));
    red->setBrush(QColor(255, 0, 0, 128));
    green->setPen(QPen(QColor(0, 255, 0, 128)));
    green->setBrush(QColor(0, 255, 0, 128));
    blue->setPen(QPen(QColor(0, 0, 255, 128)));
    blue->setBrush(QColor(0, 0, 255, 128));

    red->setWidth(1);
    green->setWidth(1);
    blue->setWidth(1);

    setBackground(style.palette.background());
}

void ImageHistogram::updateHistograms(QSharedPointer< Histograms > histograms, bool grayscale)
{
    red->clearData();
    green->clearData();
    blue->clearData();
    if (gray != grayscale) {
        gray = grayscale;
        if (gray) {
            QStyleOption style;
            red->setPen(QPen(style.palette.color(QPalette::Text)));
            red->setBrush(style.palette.color(QPalette::Text));
        } else {
            red->setPen(QPen(QColor(255, 0, 0, 128)));
            red->setBrush(QColor(255, 0, 0, 128));
        }
    }
    if (grayscale) {
        for (int i = 0; i < 255; i++)
            red->addData(i, histograms->red[i]);
    } else {
        for (int i = 0; i < 255; i++) {
            red->addData(i, histograms->red[i]);
            green->addData(i, histograms->green[i]);
            blue->addData(i, histograms->blue[i]);
        }
    }
    rescaleAxes();
    replot();
}
