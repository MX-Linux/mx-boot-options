/**********************************************************************
 *  mainwindow.h
 **********************************************************************
 * Copyright (C) 2017 MX Authors
 *
 * Authors: Adrian, Dolphin Oracle
 *          MX Linux <http://mxlinux.org>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/



#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMessageBox>
#include <QProgressBar>

#include <cmd.h>


namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QString getVersion(QString name) const;

    void addGrubArg(const QString &key, const QString &item);
    void enableGrubLine(const QString &item);
    void disableGrubLine(const QString &item);
    void remGrubArg(const QString &key, const QString &item);
    void replaceGrubArg(const QString &key, const QString &item);
    void loadPlymouthThemes() const;
    void readGrubCfg();
    void readDefaultGrub();
    void readKernelOpts();
    void setup();
    void writeDefaultGrub() const;

    bool checkInstalled(const QString &package) const;
    bool installSplash();
    int findMenuEntryById(const QString &id) const;


public slots:

private slots:
    void cleanup();
    void cmdStart();
    void cmdDone();
    void procTime(int counter, int);
    void setConnections();
    void on_buttonApply_clicked();
    void on_buttonAbout_clicked();
    void on_buttonHelp_clicked();
    void on_cb_bootsplash_clicked(bool checked);
    void on_button_filename_clicked();
    void on_rb_detailed_msg_toggled(bool checked);
    void on_rb_very_detailed_msg_toggled(bool checked);
    void on_rb_limited_msg_toggled(bool checked);
    void on_rb_predefined_toggled(bool checked);
    void on_rb_lastbooted_toggled(bool checked);
    void on_spinBoxTimeout_valueChanged(int val);
    void on_combo_menu_entry_currentIndexChanged(int index);
    void on_cb_bootsplash_toggled(bool checked);
    void on_buttonLog_clicked();
    void on_combo_theme_activated(int);

protected:
    void keyPressEvent(QKeyEvent* event);
    QProgressBar *bar;

private:
    Ui::MainWindow *ui;
    Cmd *cmd;

    bool options_changed;
    bool splash_changed;
    bool messages_changed;

    QStringList grub_cfg;
    QStringList default_grub;
    QString kernel_options;
};


#endif

