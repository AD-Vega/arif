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
#include "qcustomplot.h"
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
    QWidget(parent),
    longGraphMean(boost::accumulators::tag::rolling_window::window_size = n)
{
    qcp = new QCustomPlot(this);
    auto lay = new QHBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    setLayout(lay);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(qcp);

    QStyleOption style;
    shortGraph = qcp->addGraph(qcp->xAxis2, qcp->yAxis);
    longGraph = qcp->addGraph(qcp->xAxis, qcp->yAxis);

    qcp->xAxis->setLabel("Frame number (all frames)");
    qcp->xAxis->setLabelColor(color1);
    qcp->xAxis->setTickLabelColor(color1);
    qcp->xAxis->setTickPen(QPen(color1));
    qcp->xAxis->setBasePen(QPen(color1));

    qcp->xAxis2->setLabel(QString("Frame number (last %1 frames)").arg(shortLength));
    qcp->xAxis2->setVisible(true);
    qcp->xAxis2->setTickPen(QPen(color2));
    qcp->xAxis2->setLabelColor(color2);
    qcp->xAxis2->setTickLabelColor(color2);
    qcp->xAxis2->setTickPen(QPen(color2));
    qcp->xAxis2->setBasePen(QPen(color2));

    qcp->yAxis->setLabel("Quality");
    qcp->yAxis->setLabelColor(style.palette.color(QPalette::Text));
    qcp->yAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    qcp->yAxis->setTickPen(style.palette.color(QPalette::Text));
    qcp->yAxis->setBasePen(style.palette.color(QPalette::Text));

    longGraph->setPen(QPen(color1));
    shortGraph->setPen(QPen(color2));
    qcp->setBackground(style.palette.background());

    showSamplingText();

    qcp->setNotAntialiasedElements(QCP::aeAll);
    qcp->setPlottingHints(QCP::phFastPolylines | QCP::phCacheLabels);
}

void QualityGraph::setShortGraphMaxFrames(int frames)
{
    shortLength = frames;
    if (shortGraph->data()->size() > shortLength) {
        shortGraph->data()->removeBefore(counter - shortLength);
    }
    qcp->xAxis2->setLabel(QString("Frame number (last %1 frames)").arg(shortLength));
    qcp->replot(QCustomPlot::rpQueuedReplot);
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
            shortGraph->data()->removeBefore(counter - shortLength);
        }
    }
}

void QualityGraph::addLine()
{
    QPen pen(Qt::DashLine);
    pen.setColor(color1);
    auto line = new QCPItemStraightLine(qcp);
    line->setPen(pen);
    line->point1->setCoords(counter, 0);
    line->point2->setCoords(counter, 1);
}

void QualityGraph::clear()
{
    counter = 0;
    longGraph->data()->clear();
    qcp->clearItems();
    shortGraph->data()->clear();
    showSamplingText();
    qcp->replot(QCustomPlot::rpQueuedReplot);
}

void QualityGraph::draw()
{
    if (samplingLabel) {
        qcp->removeItem(samplingLabel);
        samplingLabel = nullptr;
    }
    qcp->rescaleAxes();
    qcp->replot(QCustomPlot::rpQueuedReplot);
}

void QualityGraph::showSamplingText()
{
    samplingLabel = createSamplingLabel(qcp);
}

QualityHistogram::QualityHistogram(QWidget* parent):
    QWidget(parent),
    accumulator(boost::accumulators::tag::density::cache_size = n,
                boost::accumulators::tag::density::num_bins = N)
{
    qcp = new QCustomPlot(this);
    auto lay = new QHBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    setLayout(lay);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(qcp);

    QStyleOption style;
    graph = new QCPBars(qcp->xAxis, qcp->yAxis);

    qcp->xAxis->setLabel("Quality of frames");
    qcp->xAxis->setLabelColor(style.palette.color(QPalette::Text));
    qcp->xAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    qcp->xAxis->setTickPen(QPen(style.palette.color(QPalette::Text)));
    qcp->xAxis->setBasePen(QPen(style.palette.color(QPalette::Text)));

    qcp->yAxis->setLabel("Percentage of frames");
    qcp->yAxis->setLabelColor(style.palette.color(QPalette::Text));
    qcp->yAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    qcp->yAxis->setTickPen(style.palette.color(QPalette::Text));
    qcp->yAxis->setBasePen(style.palette.color(QPalette::Text));

    QColor tmp = style.palette.color(QPalette::Text);
    tmp.setAlpha(64);
    graph->setPen(QPen(style.palette.color(QPalette::Text)));
    graph->setBrush(tmp);
    qcp->setBackground(style.palette.background());

    showSamplingText();

    qcp->setNotAntialiasedElements(QCP::aeAll);
    qcp->setPlottingHints(QCP::phFastPolylines | QCP::phCacheLabels);
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
    graph->data()->clear();
    showSamplingText();
    qcp->replot(QCustomPlot::rpQueuedReplot);
}

void QualityHistogram::draw()
{
    if (counter > (decltype(counter))n) {
        auto histogram = boost::accumulators::density(accumulator);
        if (samplingLabel) {
            qcp->removeItem(samplingLabel);
            samplingLabel = nullptr;
            // Set bar width.
            auto v2 = histogram.begin();
            auto v1 = v2;
            ++v1;
            graph->setWidth((v2->first - v1->first));
        }
        graph->data()->clear();
        for (auto& h : histogram) {
            graph->addData(h.first, h.second*100);
        }
        qcp->rescaleAxes();
        qcp->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QualityHistogram::showSamplingText()
{
    samplingLabel = createSamplingLabel(qcp);
}

ImageHistogram::ImageHistogram(QWidget* parent): QWidget(parent)
{
    qcp = new QCustomPlot(this);
    auto lay = new QHBoxLayout;
    setContentsMargins(0, 0, 0, 0);
    setLayout(lay);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(qcp);

    QStyleOption style;
    red = new QCPBars(qcp->xAxis, qcp->yAxis);
    green = new QCPBars(qcp->xAxis, qcp->yAxis);
    blue = new QCPBars(qcp->xAxis, qcp->yAxis);
    green->moveAbove(blue);
    red->moveAbove(green);

    qcp->xAxis->setTicks(false);
    qcp->xAxis->setTickLabels(false);
    qcp->xAxis->setBasePen(style.palette.color(QPalette::Text));

    qcp->yAxis->setTicks(false);
    qcp->yAxis->setTickLabels(false);
    qcp->yAxis->setBasePen(style.palette.color(QPalette::Text));

    red->setPen(QPen(QColor(255, 0, 0, 128)));
    red->setBrush(QColor(255, 0, 0, 128));
    green->setPen(QPen(QColor(0, 255, 0, 128)));
    green->setBrush(QColor(0, 255, 0, 128));
    blue->setPen(QPen(QColor(0, 0, 255, 128)));
    blue->setBrush(QColor(0, 0, 255, 128));

    red->setWidth(1);
    green->setWidth(1);
    blue->setWidth(1);

    qcp->setBackground(style.palette.background());
}

void ImageHistogram::updateHistograms(QSharedPointer< Histograms > histograms, bool grayscale)
{
    red->data()->clear();
    green->data()->clear();
    blue->data()->clear();
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
    qcp->rescaleAxes();
    qcp->replot(QCustomPlot::rpQueuedReplot);
}
