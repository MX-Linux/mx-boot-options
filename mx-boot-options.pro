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
CONFIG   += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mx-boot-options
TEMPLATE = app


SOURCES += main.cpp\
    mainwindow.cpp \
    dialog.cpp \
    cmd.cpp

HEADERS  += \
    mainwindow.h \
    dialog.h \
    version.h \
    cmd.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/mx-boot-options_am.ts \
                translations/mx-boot-options_ar.ts \
                translations/mx-boot-options_bg.ts \
                translations/mx-boot-options_ca.ts \
                translations/mx-boot-options_cs.ts \
                translations/mx-boot-options_da.ts \
                translations/mx-boot-options_de.ts \
                translations/mx-boot-options_el.ts \
                translations/mx-boot-options_es.ts \
                translations/mx-boot-options_et.ts \
                translations/mx-boot-options_eu.ts \
                translations/mx-boot-options_fa.ts \
                translations/mx-boot-options_fi.ts \
                translations/mx-boot-options_fr.ts \
                translations/mx-boot-options_he_IL.ts \
                translations/mx-boot-options_hi.ts \
                translations/mx-boot-options_hr.ts \
                translations/mx-boot-options_hu.ts \
                translations/mx-boot-options_id.ts \
                translations/mx-boot-options_is.ts \
                translations/mx-boot-options_it.ts \
                translations/mx-boot-options_ja.ts \
                translations/mx-boot-options_kk.ts \
                translations/mx-boot-options_ko.ts \
                translations/mx-boot-options_lt.ts \
                translations/mx-boot-options_mk.ts \
                translations/mx-boot-options_mr.ts \
                translations/mx-boot-options_nb.ts \
                translations/mx-boot-options_nl.ts \
                translations/mx-boot-options_pl.ts \
                translations/mx-boot-options_pt.ts \
                translations/mx-boot-options_pt_BR.ts \
                translations/mx-boot-options_ro.ts \
                translations/mx-boot-options_ru.ts \
                translations/mx-boot-options_sk.ts \
                translations/mx-boot-options_sl.ts \
                translations/mx-boot-options_sq.ts \
                translations/mx-boot-options_sr.ts \
                translations/mx-boot-options_sv.ts \
                translations/mx-boot-options_tr.ts \
                translations/mx-boot-options_uk.ts \
                translations/mx-boot-options_zh_CN.ts \
                translations/mx-boot-options_zh_TW.ts

RESOURCES += \
    images.qrc

