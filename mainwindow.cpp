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
#include <QKeyEvent>
#include <QTextEdit>
#include <QProgressDialog>

#include <QDebug>


MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Return the name of the defualt theme
void MainWindow::loadPlymouthThemes() const
{
    // load combobox
    ui->combo_theme->clear();;
    ui->combo_theme->addItems(cmd->getOutput("plymouth-set-default-theme -l").split("\n"));

    // get current theme
    QString current_theme = cmd->getOutput("plymouth-set-default-theme");
    if (!current_theme.isEmpty()) {
        ui->combo_theme->setCurrentIndex(ui->combo_theme->findText(current_theme));
    }
}

// Process keystrokes
void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (!cmd->isRunning()) {
            return qApp->quit();
        } else {
            int ans = QMessageBox::question(this, tr("Still running") , "Process still running. Are you sure you want to quit?");
            if (ans == QMessageBox::Yes) {
                return qApp->quit();
            }
        }
    }
}

// Setup versious items first time program runs
void MainWindow::setup()
{
    cmd = new Cmd(this);
    bar = 0;
    options_changed = false;
    splash_changed = false;
    messages_changed =false;

    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::cleanup);
    this->setWindowTitle("MX Boot Options");
    ui->buttonCancel->setEnabled(true);
    ui->buttonApply->setEnabled(true);
    ui->label_theme->setDisabled(true);
    ui->combo_theme->setDisabled(true);
    ui->button_preview->setDisabled(true);
    ui->cb_enable_flatmenus->setEnabled(true);
    readGrubCfg();
    readDefaultGrub();
    readKernelOpts();
    ui->rb_limited_msg->setVisible(!ui->cb_bootsplash->isChecked());

    // hide limited msg option if running systemd
    if (system("pgrep systemd -ns 1 2>&1 >/dev/null") == 0) {
        ui->rb_limited_msg->setVisible(false);
    }

    ui->buttonApply->setDisabled(true);
    this->adjustSize();
}


// Checks if package is installed
bool MainWindow::checkInstalled(const QString &package) const
{
    //qDebug() << "+++ Enter Function:" << __PRETTY_FUNCTION__ << "+++";
    QString cmdstr = QString("dpkg -s %1 | grep Status").arg(package);
    if (cmd->getOutput(cmdstr) == "Status: install ok installed") {
        return true;
    }
    return false;
}


// Install Bootsplash
bool MainWindow::installSplash()
{
    QProgressDialog *progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);

    QString packages = "plymouth plymouth-x11 plymouth-themes";
    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(0);
    progress->setWindowTitle(tr("Installing bootsplash, please wait"));
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->resize(500, progress->height());
    progress->show();

    setConnections();
    progress->setLabelText(tr("Updating sources"));
    cmd->run("apt-get update");
    progress->setLabelText(tr("Installing") + " " + packages);
    cmd->run("apt-get install -y " + packages);

    if (cmd->getExitCode() != 0) {
        progress->close();
        QMessageBox::critical(this, tr("Error"), tr("Could not install the bootsplash."));
        ui->cb_bootsplash->setChecked(false);
        return false;
    }

    progress->close();
    return true;
}


// Write new config in /etc/default/grup
void MainWindow::writeDefaultGrub() const
{
    QFile file("/etc/default/grub");
    if(!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    QTextStream stream(&file);
    foreach (const QString &line, default_grub) {
        stream << line << "\n";
    }
    file.close();
}

// find menuentry by id
int MainWindow::findMenuEntryById(const QString &id) const
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
QString MainWindow::getVersion(QString name) const
{
    Cmd cmd;
    return cmd.getOutput("dpkg-query -f '${Version}' -W " + name);
}

// Add item to the key in /etc/default/grub
void MainWindow::addGrubArg(const QString &key, const QString &item)
{
    QStringList new_list;
    foreach (QString line, default_grub) {
        if (line.contains(key)) {               // find key
            if (line.contains(item)) {          // return if already has the item
                return;
            } else if (line.endsWith("=")) {    // empty line terminated in equal
                line.append(item);
            } else if (line.endsWith("\"\"")) { // line that ends with a double quote
                line.chop(1); // chop last quote
                line.append(item).append("\"");
            } else if (line.endsWith("\"")) {   // line ends with one quote (has other elements
                line.chop(1); // chop last quote
                line.append(" ").append(item).append("\"");
            } else if (line.endsWith("''")) {    // line ends with 2 single quotes
                line.chop(1); // chop last quote
                line.append(item).append("'");
            } else if     (line.endsWith("'")) {    // line ends with a single quote
                line.chop(1); // chop last quote
                line.append(" ").append(item).append("'");
            } else {                            // line ends with another item
                line.append(" ").append(item);
            }
        }
        new_list << line;
    }
    default_grub = new_list;
}

// uncomment or add line in /etc/default/grub
void MainWindow::enableGrubLine(const QString &item)
{
    bool found = false;
    QStringList new_list;
    foreach (QString line, default_grub) {
        if (line == item) {
            found = true;
        } else if (line.contains(QRegularExpression("^#.*" + item))) { // if commented out
            found = true;
            line = item;
        }
        new_list << line;
    }
    if (found) {
        default_grub = new_list;
    } else {
        default_grub << "\n" << item;
    }
}

// comment out line in /etc/default/grub
void MainWindow::disableGrubLine(const QString &item)
{
    QStringList new_list;
    foreach (QString line, default_grub) {
        if (line == item) {
            line = "#" + item;
        }
        new_list << line;
    }
    default_grub = new_list;
}

// Remove itme from key in /etc/default/grub
void MainWindow::remGrubArg(const QString &key, const QString &item)
{
    QStringList new_list;
    foreach (QString line, default_grub) {
        if (line.contains(key)) { // find key
            line.remove(QRegularExpression("\\s*" + item));
        }
        new_list << line;
    }
    default_grub = new_list;
}

// Replace the argument in /etc/default/grub
void MainWindow::replaceGrubArg(const QString &key, const QString &item)
{
    QStringList new_list;
    foreach (QString line, default_grub) {
        if (line.contains(key)) { // find key
            line = key + "=" + item;
        }
        new_list << line;
    }
    default_grub = new_list;
}

// Read and parse grub.cfg file
void MainWindow::readGrubCfg()
{
    QFile file("/boot/grub/grub.cfg");
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    ui->combo_menu_entry->clear();

    QString menu_id;
    QString line;
    while (!file.atEnd()) {
        line = file.readLine().trimmed();
        grub_cfg << line;
        if (line.startsWith("menuentry ") || line.startsWith("submenu ")) {
            menu_id = line.section("$menuentry_id_option", 1, -1).section(" ", 1, 1);
            ui->combo_menu_entry->addItem(line.section(QRegularExpression("['\"]"), 1, 1), menu_id);
        }
    }
    file.close();
}

// Read default grub config file
void MainWindow::readDefaultGrub()
{
    QFile file("/etc/default/grub");
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    QString line;
    while (!file.atEnd()) {
        line = file.readLine().trimmed();
        default_grub << line;
        if (line.startsWith("GRUB_DEFAULT=")) {
            QString entry = line.section("=", 1, -1);
            bool ok;
            int number = entry.toInt(&ok);
            if (ok) {
                ui->combo_menu_entry->setCurrentIndex(number);
            } else if (entry == "saved") {
                ui->cb_save_default->setChecked(true);
            } else if (entry.size() > 1) {  // if not saved but still long word assume it's a menuendtry_id or menuentry_name
                int index = ui->combo_menu_entry->findData(entry);
                if (index != -1) { // menuentry_id
                    ui->combo_menu_entry->setCurrentIndex(index);
                } else {           // menuentry_name most likely
                    ui->combo_menu_entry->setCurrentIndex(ui->combo_menu_entry->findText(entry.remove("'").remove("\"")));
                }
            }
        } else if (line.startsWith("GRUB_TIMEOUT=")) {
            ui->spinBoxTimeout->setValue(line.section("=", 1, 1).toInt());
        } else if (line.startsWith("export GRUB_MENU_PICTURE=")) {
            ui->button_filename->setText(line.section("=", 1, 1).remove("\""));
        } else if (line.startsWith("GRUB_CMDLINE_LINUX_DEFAULT=")) {
            QString entry = line.section("=", 1, -1);
            if (entry.contains("hush")) {
                ui->rb_limited_msg->setChecked(true);
            } else if (entry.contains("quiet")) {
                ui->rb_detailed_msg->setChecked(true);
            } else {
                ui->rb_very_detailed_msg->setChecked(true);
            }
            ui->cb_bootsplash->setChecked(entry.contains("splash"));
        } else if (line == "GRUB_DISABLE_SUBMENU=y") {
            ui->cb_enable_flatmenus->setChecked(true);
        }
    }
}

// Read kernel line and options from /proc/cmdline
void MainWindow::readKernelOpts()
{
    QFile file("/proc/cmdline");
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    kernel_options = file.readAll();
}

void MainWindow::cmdStart()
{
    setCursor(QCursor(Qt::BusyCursor));
}


void MainWindow::cmdDone()
{
    setCursor(QCursor(Qt::ArrowCursor));
    if (bar) {
        bar->setValue(100);
    }
}

void MainWindow::procTime(int counter, int)
{
    if (bar != 0) {
        if (bar->value() == 100) {
            bar->reset();
        }
        bar->setValue(counter);
        qApp->processEvents();
    }
}

// set proc and timer connections
void MainWindow::setConnections()
{
    cmd->disconnect();
    connect(cmd, &Cmd::started, this, &MainWindow::cmdStart);
    connect(cmd, &Cmd::finished, this, &MainWindow::cmdDone);
    connect(cmd, &Cmd::runTime, this, &MainWindow::procTime);
}


// Next button clicked
void MainWindow::on_buttonApply_clicked()
{
    ui->buttonCancel->setDisabled(true);
    ui->buttonApply->setDisabled(true);
    QProgressDialog *progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);

    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(0);
    progress->setWindowTitle(tr("Updating configuration, please wait"));
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->resize(500, progress->height());
    progress->show();
    setConnections();

    if (options_changed) {
        replaceGrubArg("GRUB_TIMEOUT", QString::number(ui->spinBoxTimeout->value()));
        replaceGrubArg("export GRUB_MENU_PICTURE", "\"" + ui->button_filename->text() + "\"");
        if (ui->cb_enable_flatmenus->isChecked()) { // for simple menu index number is sufficient
            replaceGrubArg("GRUB_DEFAULT", QString::number(ui->combo_menu_entry->currentIndex()));
        } else {  // if submenus exists then use menuentry_id
            if (!ui->combo_menu_entry->currentData().isNull()) {
                replaceGrubArg("GRUB_DEFAULT", ui->combo_menu_entry->currentData().toString());
            } else if (ui->combo_menu_entry->currentText().contains("memtest")) { // if menuentry_id is empty most likely memtest
                replaceGrubArg("GRUB_DEFAULT", "\"" + ui->combo_menu_entry->currentText() + "\"");
            }
        }
        if (ui->cb_save_default->isChecked()) {
            replaceGrubArg("GRUB_DEFAULT", "saved");
            enableGrubLine("GRUB_SAVEDEFAULT=true");
            cmd->run("grub-set-default " + ui->combo_menu_entry->currentData().toString());
        } else {
            disableGrubLine("GRUB_SAVEDEFAULT=true");
        }
    }
    if (splash_changed) {
        if (ui->cb_bootsplash->isChecked()) {
            addGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "splash");
            if (!ui->combo_theme->currentText().isEmpty()) {
                cmd->run("plymouth-set-default-theme " + ui->combo_theme->currentText());
            }
            cmd->run("update-rc.d bootlogd disable");
        } else {
            remGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "splash");
            cmd->run("update-rc.d bootlogd enable");
        }
        progress->setLabelText(tr("Updating initramfs..."));
        cmd->run("update-initramfs -u -k all");
    }
    if (messages_changed) {
        if (ui->rb_detailed_msg->isChecked()) { // remove "hush", add "quiet" if not present
            addGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "quiet");
            remGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "hush");
        } else if (ui->rb_limited_msg->isChecked()) { // add "quiet" and "hush" to /boot/default/grub
            addGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "quiet");
            addGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "hush");
        } else if (ui->rb_very_detailed_msg->isChecked()) { // remove "hush" and/or "quiet"
            remGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "quiet");
            remGrubArg("GRUB_CMDLINE_LINUX_DEFAULT", "hush");
        }
    }
    if (options_changed || splash_changed || messages_changed) {
        writeDefaultGrub();
        progress->setLabelText(tr("Updating grub..."));
        cmd->run("update-grub");
        progress->close();
        QMessageBox::information(this, tr("Done") , tr("Changes have been sucessfully applied."));
    }
    options_changed = false;
    splash_changed = false;
    messages_changed = false;
    ui->buttonCancel->setEnabled(true);
}


// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About") + " MX Boot Options", "<p align=\"center\"><b><h2>MX Boot Options</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + getVersion("mx-boot-options") + "</p><p align=\"center\"><h3>" +
                       tr("Program for selecting common start-up choices") +
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
    QString url = "https://mxlinux.org/wiki/help-files/help-boot-options";
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


void MainWindow::on_cb_bootsplash_clicked(bool checked)
{
    if (checked) {
        if (!checkInstalled("plymouth") || !checkInstalled("plymouth-x11") || !checkInstalled("plymouth-themes")) {
            int ans = QMessageBox::question(this, tr("Plymouth not installed"), tr("Plymouth bootloader is not installed.\nOK to go ahead and install it?"));
            if (ans == QMessageBox::No) {
                ui->cb_bootsplash->setChecked(false);
                return;
            }
            installSplash();
        }
        loadPlymouthThemes();
        splash_changed = true;
        ui->buttonApply->setEnabled(true);
        on_buttonApply_clicked();
    }
    splash_changed = true;
    ui->buttonApply->setEnabled(true);
    if (checked && ui->rb_limited_msg->isChecked()) {
        ui->rb_detailed_msg->setChecked(true);
    }
    ui->rb_limited_msg->setVisible(!checked);
    ui->buttonApply->setEnabled(true);
}

void MainWindow::on_button_filename_clicked()
{
    QString selected = QFileDialog::getOpenFileName(this, QObject::tr("Select image to display in bootloader"),
                                              "/usr/share/backgrounds/MXLinux/grub", tr("Images (*.png *.jpg *.jpeg *.tga)"));
    if (!selected.isEmpty()) {
        ui->button_filename->setText(selected);
        options_changed = true;
        ui->buttonApply->setEnabled(true);
    }
}


void MainWindow::on_rb_detailed_msg_toggled(bool checked)
{
    if (checked) {
        messages_changed = true;
        ui->buttonApply->setEnabled(true);
    }
}

void MainWindow::on_rb_very_detailed_msg_toggled(bool checked)
{
    if (checked) {
        messages_changed = true;
        ui->buttonApply->setEnabled(true);
    }
}

void MainWindow::on_rb_limited_msg_toggled(bool checked)
{
    if (checked) {
        messages_changed = true;
        ui->buttonApply->setEnabled(true);
    }
}

void MainWindow::on_spinBoxTimeout_valueChanged(int)
{
    options_changed = true;
    ui->buttonApply->setEnabled(true);
}

void MainWindow::on_combo_menu_entry_currentIndexChanged(int)
{
    options_changed = true;
    ui->buttonApply->setEnabled(true);
}


// Toggled either by user or when reading the status of bootsplash
void MainWindow::on_cb_bootsplash_toggled(bool checked)
{
    ui->combo_theme->setEnabled(checked);
    ui->label_theme->setEnabled(checked);
    ui->button_preview->setEnabled(checked);
    loadPlymouthThemes();
}

void MainWindow::on_buttonLog_clicked()
{
    QString location = "/var/log/boot";
    if (kernel_options.contains("hush")) {
        location = "/run/rc.log";
    } else if (kernel_options.contains("splash")) {
        location = "/var/log/boot.log";
    }
    QString sed = "sed 's/\\^\\[/\\x1b/g'";  // remove formatting escape char
    if (QFile::exists(location)) {
        system("x-terminal-emulator -e bash -c \"" + sed.toUtf8() + " " + location.toUtf8() + "; read -n1 -srp '"+ tr("Press and key to close").toUtf8() + "'\"&");
    } else {
        QMessageBox::critical(this, tr("Log not found"), tr("Could not find log at ") + location);
    }
}


void MainWindow::on_combo_theme_activated(int)
{
    splash_changed = true;
    ui->buttonApply->setEnabled(true);
}

void MainWindow::on_button_preview_clicked()
{
    QString current_theme = cmd->getOutput("plymouth-set-default-theme");
    if (ui->combo_theme->currentText() == "details") {
        return;
    }
    cmd->run("plymouth-set-default-theme " + ui->combo_theme->currentText());
    cmd->run("x-terminal-emulator -e bash -c 'plymouthd; plymouth --show-splash; for ((i=0; i<5; i++)); do plymouth --update=test$i; sleep 1; done; plymouth quit'");
    cmd->run("plymouth-set-default-theme " + current_theme); // return to current theme
}

void MainWindow::on_cb_enable_flatmenus_clicked(bool checked)
{
    QProgressDialog *progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);

    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(0);
    progress->setWindowTitle(tr("Updating configuration, please wait"));
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->resize(500, progress->height());
    progress->show();

    if (checked) {
        enableGrubLine("GRUB_DISABLE_SUBMENU=y");
    } else {
        disableGrubLine("GRUB_DISABLE_SUBMENU=y");
    }

    writeDefaultGrub();
    progress->setLabelText(tr("Updating grub..."));
    setConnections();
    cmd->run("update-grub");
    readGrubCfg();
    progress->close();
}

void MainWindow::on_cb_save_default_clicked()
{
    options_changed = true;
    ui->buttonApply->setEnabled(true);
}
