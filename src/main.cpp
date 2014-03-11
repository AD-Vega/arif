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

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationDomain("ad-vega.si");
    QCoreApplication::setOrganizationName("AD Vega");
    QCoreApplication::setApplicationName("arif");
    VideoSourcePlugin* plugin;
    {
        SourceSelectionWindow s;
        s.exec();
        plugin = s.result() == QDialog::Accepted ? s.selectedSource : nullptr;
    }
    if (plugin) {
        ArifMainWindow w(plugin);
        w.show();
        return a.exec();
    }
    return 1;
}
