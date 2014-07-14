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
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationDomain("ad-vega.si");
    QCoreApplication::setOrganizationName("AD Vega");
    QCoreApplication::setApplicationName("arif");
    VideoSourcePlugin* plugin;
    QWidget* control;
    QString settingsFile, videoFile, destinationDir;

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

        cmd.parse(argc, argv);
        settingsFile = QString::fromStdString(settingsArg.getValue());
        videoFile = QString::fromStdString(inputArg.getValue());
        destinationDir = QString::fromStdString(outputArg.getValue());
    } catch (TCLAP::ArgException &e) {
        std::cerr << "Error processing argument " << e.argId() << std::endl
                  << e.error() << std::endl;
        return 1;
    }

    if (!videoFile.isEmpty() && !destinationDir.isEmpty()) {
        // TODO: handle file processing
    } else if (videoFile.isEmpty() && destinationDir.isEmpty()) {
        // OK, show GUI and operate normally.
    } else {
        std::cerr << "Error: both input and output must be specified!" << std::endl;
        return 1;
    }

    {
        SourceSelectionWindow s;
        s.exec();
        plugin = s.result() == QDialog::Accepted ? s.selectedSource : nullptr;
        control = s.sourceControl;
    }
    if (plugin) {
        ArifMainWindow w(plugin, control);
        w.show();
        return a.exec();
    }
    return 1;
}
