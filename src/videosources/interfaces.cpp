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

#include "videosources/interfaces.h"
#include <QSettings>

void FrameMetaData::serialize(QDataStream& s)
{
  s << timestamp;
  s << frameOfSecond;
}

void FrameMetaData::load(QDataStream& s)
{
    s >> timestamp;
    s >> frameOfSecond;
}

void RawFrame::serialize(QDataStream& s)
{
    metaData.serialize(s);
}

void RawFrame::load(QDataStream& s)
{
    metaData.load(s);
}

FrameMetaData Reader::makeMetaData()
{
    auto now = QDateTime::currentDateTimeUtc();
    auto unixtime = now.toTime_t();
    if (unixtime != previousUnixtime) {
        previousUnixtime = unixtime;
        frameOfSecond = 0;
    }
    FrameMetaData metadata;
    metadata.timestamp = now;
    metadata.frameOfSecond = ++frameOfSecond;
    return metadata;
}

void VideoSourcePlugin::readSettings(QString file)
{
    QScopedPointer<QSettings> config;
    if (file.isEmpty())
        config.reset(new QSettings);
    else
        config.reset(new QSettings(file, QSettings::IniFormat));
    config->beginGroup(settingsGroup());
    for (auto& k: config->allKeys())
        settings.insert(k, config->value(k));
}

void VideoSourcePlugin::saveSettings(QString file)
{
    QScopedPointer<QSettings> config;
    if (file.isEmpty())
        config.reset(new QSettings);
    else
        config.reset(new QSettings(file, QSettings::IniFormat));
    config->beginGroup(settingsGroup());
    for (auto i = settings.constBegin(), end = settings.constEnd();
         i != end; ++i)
         config->setValue(i.key(), i.value());
}

static int registerTypes()
{
    qRegisterMetaType<SharedRawFrame>("SharedRawFrame");
    return 0;
}

static int __register = registerTypes();