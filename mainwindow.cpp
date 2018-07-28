/**********************************************************************
 *  mainwindow.cpp
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



#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QTextEdit>

#include <QDebug>


MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup();
    readGrubCfg();
    readDefaultGrub();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// setup versious items first time program runs
void MainWindow::setup()
{
    cmd = new Cmd(this);
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::cleanup);
    this->setWindowTitle("MX Boot Options");
    this->adjustSize();
    ui->buttonCancel->setEnabled(true);
    ui->buttonApply->setEnabled(true);
    ui->label_theme->setDisabled(true);
    ui->combo_theme->setDisabled(true);
}

// find menuentry by id
int MainWindow::findMenuEntryById(QString id)
{
    int count = 0;
    foreach (QString line, grub_cfg) {
        if (line.startsWith("menuentry ")) {
            if(line.contains("--id " + id)) {
                return count;
            }
            ++count;
        }
    }
    return 0;

}

// cleanup environment when window is closed
void MainWindow::cleanup()
{

}


// Get version of the program
QString MainWindow::getVersion(QString name)
{
    Cmd cmd;
    return cmd.getOutput("dpkg-query -f '${Version}' -W " + name);
}


// Read and parse grub.cfg file
void MainWindow::readGrubCfg()
{
    QFile file("/boot/grub/grub.cfg");
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Count not open file: " << file.fileName();
        return;
    }
    QString line;
    while (!file.atEnd()) {
        line = file.readLine().trimmed();
        grub_cfg << line;
        if (line.startsWith("menuentry ")) {
            ui->combo_menu_entry->addItem(line.section(QRegularExpression("['\"]"), 1, 1));
        }
    }
    file.close();
}

// Read default grub config file
void MainWindow::readDefaultGrub()
{
    QFile file("/etc/default/grub");
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Count not open file: " << file.fileName();
        return;
    }
    QString line;
    while (!file.atEnd()) {
        line = file.readLine().trimmed();
        default_grub << line;
        if (line.startsWith("GRUB_DEFAULT=")) {
            QString entry = line.section("=", 1, 1);
            bool ok;
            int number = entry.toInt(&ok);
            if (ok) {
                ui->combo_menu_entry->setCurrentIndex(number);
            } else if (entry == "saved") {
                ui->rb_lastbooted->setChecked(true);
            } else if (entry.size() > 1) {  // if not saved but still long word assume it's a GRUB id
                ui->combo_menu_entry->setCurrentIndex(findMenuEntryById(entry));
            }
        } else if (line.startsWith("GRUB_TIMEOUT=")) {
            ui->spinBoxTimeout->setValue(line.section("=", 1, 1).toInt());
        }
    }
}

void MainWindow::cmdStart()
{
    setCursor(QCursor(Qt::BusyCursor));
}


void MainWindow::cmdDone()
{
    setCursor(QCursor(Qt::ArrowCursor));
}

// set proc and timer connections
void MainWindow::setConnections()
{
    cmd->disconnect();
    connect(cmd, &Cmd::started, this, &MainWindow::cmdStart);
    connect(cmd, &Cmd::finished, this, &MainWindow::cmdDone);
}


// Next button clicked
void MainWindow::on_buttonApply_clicked()
{

}


// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About") + " MX Boot Options", "<p align=\"center\"><b><h2>MX Boot Options</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + getVersion("mx-boot-options") + "</p><p align=\"center\"><h3>" +
                       tr("Description goes here") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    QPushButton *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        QString url = "file:///usr/share/doc/mx-boot-options/license.html";
        Cmd c;
        QString user = c.getOutput("logname");
        if (system("command -v mx-viewer") == 0) { // use mx-viewer if available
            system("su " + user.toUtf8() + " -c \"mx-viewer " + url.toUtf8() + " " + tr("License").toUtf8() + "\"&");
        } else {
            system("su " + user.toUtf8() + " -c \"xdg-open " + url.toUtf8() + "\"&");
        }
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog(this);
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        Cmd cmd;
        text->setText(cmd.getOutput("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz"));

        QPushButton *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    QString url = "google.com";
    QString exec = "xdg-open";
    if (system("command -v mx-viewer") == 0) { // use mx-viewer if available
        exec = "mx-viewer";
        url += " " + tr("MX Boot Options");
    }
    Cmd c;
    QString user = c.getOutput("logname");
    QString cmd = QString("su " + user + " -c \"" + exec + " " + url + "\"&");
    system(cmd.toUtf8());
}

