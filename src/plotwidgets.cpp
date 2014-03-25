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

QualityGraph::QualityGraph(QWidget* parent): QCustomPlot(parent)
{
    QStyleOption style;
    longGraph = addGraph(xAxis, yAxis);
    shortGraph = addGraph(xAxis2, yAxis);

    xAxis->setLabel("All frames");
    xAxis->setLabelColor(Qt::blue);
    xAxis->setTickLabelColor(Qt::blue);
    xAxis->setTickPen(QPen(Qt::blue));
    xAxis->setBasePen(QPen(Qt::blue));

    xAxis2->setLabel("Last few frames");
    xAxis2->setVisible(true);
    xAxis2->setTickPen(QPen(Qt::red));
    xAxis2->setLabelColor(Qt::red);
    xAxis2->setTickLabelColor(Qt::red);
    xAxis2->setTickPen(QPen(Qt::red));
    xAxis2->setBasePen(QPen(Qt::red));

    yAxis->setLabel("Quality");
    yAxis->setLabelColor(style.palette.color(QPalette::Text));
    yAxis->setTickLabelColor(style.palette.color(QPalette::Text));
    yAxis->setTickPen(style.palette.color(QPalette::Text));
    yAxis->setBasePen(style.palette.color(QPalette::Text));

    longGraph->setPen(QPen(Qt::blue));
    shortGraph->setPen(QPen(Qt::red));
    setBackground(style.palette.background());

    setNotAntialiasedElements(QCP::aeAll);
}

void QualityGraph::setShortGraphMaxFrames(uint frames)
{
    shortLength = frames;
}

void QualityGraph::addFrameStats(SharedData data)
{
    if (data->stageSuccessful
            && data->completedStages.contains("EstimateQuality")) {
        ++counter;
        longGraph->addData(counter, data->quality);
        shortGraph->addData(counter, data->quality);
        if (shortGraph->data()->size() > shortLength) {
            shortGraph->removeDataBefore(counter - shortLength);
        }
    }
}
