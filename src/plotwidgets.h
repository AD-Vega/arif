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

#ifndef PLOTWIDGETS_H
#define PLOTWIDGETS_H

#include "qcustomplot.h"
#include "processing.h"
#include <boost/accumulators/statistics/density.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/accumulators.hpp>

class QualityGraph: public QCustomPlot
{
    Q_OBJECT

public:
    explicit QualityGraph(QWidget* parent = 0);

public slots:
    void setShortGraphMaxFrames(int frames);
    void addFrameStats(SharedData data);
    void clear();
    void draw();

private:
    void showSamplingText();

private:
    QCPGraph* longGraph;
    QCPGraph* shortGraph;
    QCPItemText* samplingLabel;
    int shortLength = 100;
    unsigned long long counter = 0; // provides x values
    boost::accumulators::accumulator_set < double,
          boost::accumulators::features<boost::accumulators::tag::rolling_mean >>
          longGraphMean;
};

class QualityHistogram: public QCustomPlot
{
    Q_OBJECT

public:
    explicit QualityHistogram(QWidget* parent = 0);

public slots:
    void addFrameStats(SharedData data);
    void clear();
    void draw();

private:
    void showSamplingText();

private:
    QCPBars* graph;
    QCPItemText* samplingLabel;
    uint shortLength = 100;
    boost::accumulators::accumulator_set < double,
          boost::accumulators::features<boost::accumulators::tag::density>>
          accumulator;
    unsigned long long counter = 0;
};

class ImageHistogram: public QCustomPlot
{
    Q_OBJECT

public:
    explicit ImageHistogram(QWidget* parent = 0);
    void updateHistograms(QSharedPointer<Histograms> histograms,
                          bool grayscale);

private:
    QCPBars* red, *green, *blue;
    bool gray = false;
};


#endif
