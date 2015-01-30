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

#include "sourceselectionwindow.h"
#include <QVBoxLayout>
#include <QLayout>
#include <QPluginLoader>
#include <QSettings>
#include <QDebug>

SourceSelectionWindow::SourceSelectionWindow(QWidget* parent, Qt::WindowFlags f):
    QDialog(parent, f)
{
    setWindowIcon(QIcon(":/icons/arif.svg"));
    setLayout(new QVBoxLayout);
    selector = new QComboBox;
    layout()->addWidget(selector);
    QSettings config;
    auto previousPluginName = config.value("settings/source").toString();
    int previousPlugin = 0;
    for (auto plugin: QPluginLoader::staticInstances()) {
        auto p = qobject_cast<VideoSourcePlugin*>(plugin);
        if (p != nullptr) {
            QVariant pv;
            pv.setValue(p);
            selector->addItem(p->readableName(), pv);
            if (p->name() == previousPluginName) {
                previousPlugin = selector->count() - 1;
            }
        }
    }
    selector->setCurrentIndex(-1);
    connect(selector, SIGNAL(currentIndexChanged(int)), SLOT(changeSource()));
    selector->setCurrentIndex(previousPlugin);
    connect(this, SIGNAL(finished(int)), SLOT(saveLastPluginName()));
}

void SourceSelectionWindow::changeSource()
{
    if (currentWidget) {
        layout()->removeWidget(currentWidget);
        delete currentWidget;
        currentWidget = nullptr;
    }
    QVariant pv = selector->itemData(selector->currentIndex());
    auto p = pv.value<VideoSourcePlugin*>();
    selectedSource = p;
    if (p) {
        currentWidget = p->createConfigurationWidget();
        connect(currentWidget, SIGNAL(configurationComplete(QWidget*)),
                SLOT(acceptConfiguration(QWidget*)));
        layout()->addWidget(currentWidget);
        adjustSize();
    }
}

void SourceSelectionWindow::saveLastPluginName()
{
    QSettings config;
    QVariant pv = selector->itemData(selector->currentIndex());
    auto p = pv.value<VideoSourcePlugin*>();
    if (p) {
        config.setValue("settings/source", QVariant::fromValue(p->name()));
    }
}

void SourceSelectionWindow::acceptConfiguration(QWidget* control)
{
    sourceControl = control;
    accept();
}
