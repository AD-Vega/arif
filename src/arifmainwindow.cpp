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

#include "arifmainwindow.h"
#include <QTimer>
#include <QSettings>

ArifMainWindow::ArifMainWindow(VideoSourcePlugin* plugin,
                               QWidget* parent,
                               Qt::WindowFlags flags):
    QMainWindow(parent, flags), sourcePlugin(plugin)
{
    setupUi(this);
    restoreProgramSettings();
    // Delay initialization until a later event loop cycle.
    QTimer::singleShot(0, this, SLOT(initialize()));
}

void ArifMainWindow::initialize()
{
    
}

void ArifMainWindow::closeEvent(QCloseEvent* event)
{
    saveProgramSettings();
    QWidget::closeEvent(event);
}

void ArifMainWindow::saveProgramSettings()
{
    QSettings config;
    config.setValue("mainwindow/geometry", saveGeometry());
    config.setValue("mainwindow/state", saveState());
    config.setValue("processing/cropwidth", cropWidthBox->value());
    config.setValue("processing/saveimages", imageDestinationDirectory->text());
}

void ArifMainWindow::restoreProgramSettings()
{
    QSettings config;
    restoreGeometry(config.value("mainwindow/geometry").toByteArray());
    restoreState(config.value("mainwindow/state").toByteArray());
    cropWidthBox->setValue(config.value("processing/cropwidth").toInt());
    imageDestinationDirectory->setText(config.value("processing/saveimages").toString());
}
