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

#include <QtGui/QApplication>
#include "sourceselectionwindow.h"
#include "arifmainwindow.h"
#include "videosources/interfaces.h"
#include "tclap/CmdLine.h"
#include <QSettings>
#include <QPluginLoader>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationDomain("ad-vega.si");
    QCoreApplication::setOrganizationName("AD Vega");
    QCoreApplication::setApplicationName("arif");
    VideoSourcePlugin* plugin = nullptr;
    QWidget* control;
    QString settingsFile, videoFile, destinationDir;
    bool showGUI;

    try {
        const char description[] =
            "Use the --settings option to load a previously saved settings "
            "file. If it is absent, the settings from the last time the GUI "
            "was used are loaded."
            "\n"
            "The --input and --output options must be used together. "
            "If they are absent, the GUI is started. Otherwise, --input "
            "specifies the input video path and --output specifies the output "
            "directory where processed images are placed. The input path can "
            "be anything that is compatible with the input plugin specified "
            "by the loaded settings, but must be a seekable source, e.g. "
            "a video file, image directory or similar. The input will be "
            "processed as if the 'Process entire file' option in the GUI "
            "was selected.";
        TCLAP::CmdLine cmd(description);

        TCLAP::ValueArg<std::string>
        settingsArg("s", "settings", "Settings file to load",
                    false, std::string{}, "file", cmd);
        TCLAP::ValueArg<std::string>
        inputArg("i", "input", "Input video path",
                 false, std::string{}, "video", cmd);
        TCLAP::ValueArg<std::string>
        outputArg("o", "output", "Output directory for processed images",
                  false, std::string{}, "directory", cmd);
        TCLAP::SwitchArg
        guiArg("g", "gui", "Show GUI even when batch processing", cmd);

        cmd.parse(argc, argv);
        settingsFile = QString::fromStdString(settingsArg.getValue());
        videoFile = QString::fromStdString(inputArg.getValue());
        destinationDir = QString::fromStdString(outputArg.getValue());
        showGUI = guiArg.getValue();
    } catch (TCLAP::ArgException &e) {
        std::cerr << "Error processing argument " << e.argId() << std::endl
                  << e.error() << std::endl;
        return 1;
    }

    if (!videoFile.isEmpty() && !destinationDir.isEmpty()) {
        // Handle file processing.
        QScopedPointer<QSettings> config;
        if (settingsFile.isEmpty())
            config.reset(new QSettings);
        else
            config.reset(new QSettings(settingsFile, QSettings::IniFormat));
        auto previousPluginName = config->value("settings/source").toString();
        config.reset();
        for (auto pluginPtr : QPluginLoader::staticInstances()) {
            auto p = qobject_cast<VideoSourcePlugin*>(pluginPtr);
            if (p != nullptr && p->name() == previousPluginName) {
                plugin = p;
                break;
            }
        }
        if (!plugin) {
            std::cerr << "Error: video input plugin not found!" << std::endl;
            return 1;
        }
        plugin->readSettings(settingsFile);
        auto init = plugin->initialize(videoFile);
        if (!init.isEmpty()) {
            std::cerr << (QString("Error: input initialization failed: ") + init).toStdString() << std::endl;
            return 1;
        }
        if (plugin->reader()->isSequential()) {
            std::cerr << "Error: only seekable videos are supported in batch mode!" << std::endl;
            return 1;
        }
        ArifMainWindow w(plugin, nullptr, settingsFile, destinationDir);
        if (showGUI)
            w.show();
        a.processEvents();
        w.acceptanceEntireFileCheck->setChecked(true);
        a.processEvents();
        w.processButton->setChecked(true);
        return a.exec();
    } else if (videoFile.isEmpty() && destinationDir.isEmpty()) {
        // OK, show GUI and operate normally.
        SourceSelectionWindow s;
        s.exec();
        plugin = s.result() == QDialog::Accepted ? s.selectedSource : nullptr;
        control = s.sourceControl;
    } else {
        std::cerr << "Error: both input and output must be specified!" << std::endl;
        return 1;
    }

    if (plugin) {
        ArifMainWindow w(plugin, control);
        w.show();
        return a.exec();
    }
    return 1;
}
