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

#include "glhistogramwidget.h"
#include <QStyleOption>

GLHistogramWidget::GLHistogramWidget(QWidget* parent) :
    QGLWidget(),
    histograms(new Histograms), unused(new Histograms),
    histRed(histograms->red), histGreen(histograms->green), histBlue(histograms->blue)
{
    for (int i = 0; i < 256; i++) {
        histograms->red[i] = 0;
        histograms->green[i] = 0;
        histograms->blue[i] = 0;
    }
    QFile iconfile("/usr/share/qarv/1/view-object-histogram-linear.svgz");
    if (iconfile.exists())
        idleImageIcon = QIcon(iconfile.fileName());
    else
        idleImageIcon = QIcon::fromTheme("view-object-histogram-linear");
    setIdle();
}

void GLHistogramWidget::setIdle()
{
    idle = true;
    update();
    return;
}

QSharedPointer<Histograms>& GLHistogramWidget::unusedHistograms()
{
    return unused;
}

void GLHistogramWidget::swapHistograms(bool grayscale)
{
    idle = false;
    gray = grayscale;
    auto tmp = histograms;
    histograms = unused;
    unused = tmp;
    histRed = histograms->red;
    histGreen = histograms->green;
    histBlue = histograms->blue;
    update();
}

void GLHistogramWidget::paintGL()
{
    if (idle) {
        QPainter painter(this);
        painter.drawPixmap(rect(), idleImageIcon.pixmap(rect().size()));
        return;
    }

    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    painter.setBackground(opt.palette.color(QPalette::Background));
    painter.fillRect(rect(), opt.palette.color(QPalette::Background));

    float wUnit = rect().width() / 256.;
    QPointF origin = rect().bottomLeft();

    if (gray) {
        painter.setPen(opt.palette.color(QPalette::WindowText));
        painter.setBrush(QBrush(opt.palette.color(QPalette::WindowText)));

        float max = 0;
        for (int i = 0; i < 256; i++)
            if (histRed[i] > max) max = histRed[i];
        float hUnit = rect().height() / max;
        for (int i = 0; i < 256; i++) {
            float height = histRed[i] * hUnit;
            QPointF topLeft(origin + QPointF(i * wUnit, -height));
            QPointF bottomRight(origin + QPointF((i + 1)*wUnit, 0));
            painter.drawRect(QRectF(topLeft, bottomRight));
        }
    } else {
        QColor colors[] = {
            QColor::fromRgba(qRgba(255, 0, 0, 128)),
            QColor::fromRgba(qRgba(0, 255, 0, 128)),
            QColor::fromRgba(qRgba(0, 0, 255, 128))
        };
        float* histograms[] = { histRed, histGreen, histBlue };
        float max = 0;
        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < 256; i++)
                if (histograms[c][i] > max) max = histograms[c][i];
        }
        for (int c = 0; c < 3; c++) {
            painter.setPen(colors[c]);
            painter.setBrush(colors[c]);

            float hUnit = rect().height() / max;
            for (int i = 0; i < 256; i++) {
                float height = histograms[c][i] * hUnit;
                QPointF topLeft(origin + QPointF(i * wUnit, -height));
                QPointF bottomRight(origin + QPointF((i + 1)*wUnit, 0));
                painter.drawRect(QRectF(topLeft, bottomRight));
            }
        }
    }
}
