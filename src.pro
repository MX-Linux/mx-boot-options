# **********************************************************************
# * Copyright (C) 2017 MX Authors
# *
# * Authors: Adrian, Dolphin Oracle
# *          MX Linux <http://mxlinux.org>
# *
# * This is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this package. If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mx-boot-options
TEMPLATE = app


SOURCES += main.cpp\
    mainwindow.cpp \
    dialog.cpp

HEADERS  += \
    mainwindow.h \
    dialog.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/mx-boot-options_ca.ts \
                translations/mx-boot-options_de.ts \
                translations/mx-boot-options_el.ts \
                translations/mx-boot-options_es.ts \
                translations/mx-boot-options_fr.ts \
                translations/mx-boot-options_it.ts \
                translations/mx-boot-options_ja.ts \
                translations/mx-boot-options_nl.ts \
                translations/mx-boot-options_ro.ts \
                translations/mx-boot-options_sv.ts

RESOURCES += \
    images.qrc

unix:!macx: LIBS += -lcmd
