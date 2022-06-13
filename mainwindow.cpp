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

#include <QDebug>
#include <QFileDialog>
#include <QKeyEvent>
#include <QProgressDialog>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <chrono>

using namespace std::chrono_literals;
extern const QString starting_home;

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    qDebug().noquote() << qApp->applicationName() << "version:" << qApp->applicationVersion();
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    setGeneralConnections();
    setup();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadPlymouthThemes()
{
    // load combobox
    ui->comboTheme->clear();
    ui->comboTheme->addItems(cmd.getCmdOut(chroot + "plymouth-set-default-theme -l").split(QStringLiteral("\n")));

    // get current theme
    QString current_theme = cmd.getCmdOut(chroot + "plymouth-set-default-theme");
    if (!current_theme.isEmpty())
        ui->comboTheme->setCurrentIndex(ui->comboTheme->findText(current_theme));
}

// Process keystrokes
void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (cmd.state() == QProcess::Running || cmd.state() == QProcess::Starting) {
            if (QMessageBox::Yes != QMessageBox::question(this, tr("Still running"),
                    tr("Process still running. Are you sure you want to quit?")))
                return;
        }
        qApp->quit();
    }
}

// Setup versious items first time program runs
void MainWindow::setup()
{
    chroot = QString();
    bar = nullptr;
    options_changed = false;
    splash_changed = false;
    messages_changed = false;
    just_installed = false;

    user = cmd.getCmdOut(QStringLiteral("logname"));

    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::cleanup);

    this->setWindowTitle(QStringLiteral("MX Boot Options"));
    ui->pushCancel->setEnabled(true);
    ui->pushApply->setEnabled(true);
    ui->comboTheme->setDisabled(true);
    ui->pushPreview->setDisabled(true);
    ui->checkEnableFlatmenus->setEnabled(true);

    if (QFile::exists(QStringLiteral("/boot/grub/themes"))) {
        ui->checkGrubTheme->setVisible(true);
        ui->pushThemeFile->setVisible(true);
    } else {
        ui->checkGrubTheme->setVisible(false);
        ui->pushThemeFile->setVisible(false);
    }
    ui->pushThemeFile->setDisabled(true);

    // if running live read linux partitions and set chroot on the selected one
    if (cmd.run(QStringLiteral("mountpoint -q /live/aufs"))) {
        QString part = selectPartiton(getLinuxPartitions());
        createChrootEnv(part);
    }

    if (!cmd.run(QStringLiteral("dpkg -s grub-common | grep -q 'Status: install ok installed'"))) {
        grub_installed = false;
        ui->groupBoxOptions->setHidden(true);
        ui->groupBoxBackground->setHidden(true);
    } else {
        grub_installed = true;
        readGrubCfg();
        readDefaultGrub();
    }

    readKernelOpts();
    ui->radioLimitedMsg->setVisible(!ui->checkBootsplash->isChecked());
    if (inVirtualMachine())
        ui->pushPreview->setDisabled(true);
    ui->pushApply->setDisabled(true);
    this->adjustSize();
}

// Set mouse in the corner and move it to advance splash preview
void MainWindow::sendMouseEvents()
{
    QCursor::setPos(qApp->primaryScreen()->geometry().width(), qApp->primaryScreen()->geometry().height() + 1);
}

void MainWindow::setGeneralConnections()
{
    connect(ui->pushAbout, &QPushButton::clicked, this, &MainWindow::pushAbout_clicked);
    connect(ui->pushApply, &QPushButton::clicked, this, &MainWindow::pushApply_clicked);
    connect(ui->pushHelp, &QPushButton::clicked, this, &MainWindow::pushHelp_clicked);
    connect(ui->pushLog, &QPushButton::clicked, this, &MainWindow::pushLog_clicked);
    connect(ui->pushBgFile, &QPushButton::clicked, this, &MainWindow::btn_bg_file_clicked);
    connect(ui->pushThemeFile, &QPushButton::clicked, this, &MainWindow::btn_theme_file_clicked);
    connect(ui->pushPreview, &QPushButton::clicked, this, &MainWindow::push_preview_clicked);
    connect(ui->checkBootsplash, &QCheckBox::clicked, this, &MainWindow::combo_bootsplash_clicked);
    connect(ui->checkBootsplash, &QCheckBox::toggled, this, &MainWindow::combo_bootsplash_toggled);
    connect(ui->checkEnableFlatmenus, &QCheckBox::clicked, this, &MainWindow::combo_bootsplash_clicked);
    connect(ui->checkGrubTheme, &QCheckBox::clicked, this, &MainWindow::combo_grub_theme_toggled);
    connect(ui->checkSaveDefault, &QCheckBox::clicked, this, &MainWindow::combo_save_default_clicked);
    connect(ui->comboMenuEntry, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::combo_menu_entry_currentIndexChanged);
    connect(ui->comboTheme, qOverload<int>(&QComboBox::activated), this, &MainWindow::combo_theme_activated);
    connect(ui->comboTheme, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::combo_theme_currentIndexChanged);
    connect(ui->textKernel, &QLineEdit::textEdited, this, &MainWindow::lineEdit_kernel_textEdited);
    connect(ui->radioDetailedMsg, &QRadioButton::toggled, this, &MainWindow::radio_detailed_msg_toggled);
    connect(ui->radioLimitedMsg, &QRadioButton::toggled, this, &MainWindow::radio_limited_msg_toggled);
    connect(ui->radioVeryDetailedMsg, &QRadioButton::toggled, this, &MainWindow::radio_very_detailed_msg_toggled);
    connect(ui->spinBoxTimeout, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::spinBoxTimeout_valueChanged);
}

bool MainWindow::isInstalled(const QString &package)
{
    QString cmd_str = QString(chroot + "dpkg -s %1 | grep Status").arg(package);
    return (cmd.getCmdOut(cmd_str) == QLatin1String("Status: install ok installed"));
}

// checks if a list of packages is installed, return false if one of them is not
bool MainWindow::isInstalled(const QStringList &packages)
{
    for (const QString &package : packages)
        if (!isInstalled(package))
            return false;
    return true;
}

bool MainWindow::installSplash()
{
    auto *progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);

    const auto packages = QStringLiteral("plymouth plymouth-x11 plymouth-themes plymouth-themes-mx");
    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(nullptr);
    progress->setWindowTitle(tr("Installing bootsplash, please wait"));
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->resize(500, progress->height());
    progress->show();

    setConnections();
    progress->setLabelText(tr("Updating sources"));
    cmd.run(chroot + "apt-get update");
    progress->setLabelText(tr("Installing") + " " + packages);

    if (!cmd.run(chroot + "apt-get install -y " + packages)) {
        progress->close();
        QMessageBox::critical(this, tr("Error"), tr("Could not install the bootsplash."));
        ui->checkBootsplash->setChecked(false);
        return false;
    }

    progress->close();
    return true;
}

// Detect Virtual Machine to let user know Plymouth is not fully functional
bool MainWindow::inVirtualMachine()
{
    return (!cmd.run(QStringLiteral("test -z \"$(lspci -d 80ee:beef)\"")));
}


// Write new config in /etc/default/grup
void MainWindow::writeDefaultGrub() const
{
    QString chr = chroot.section(QStringLiteral(" "), 1, 1);
    QFile file(chr + "/etc/default/grub");

    // create a new backup file
    QFile::copy(chr + "/etc/default/grub.bak", chr + "/etc/default/grub.bak.0");
    QFile::remove(chr + "/etc/default/grub.bak");
    file.copy(chr + "/etc/default/grub.bak");

    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    QTextStream stream(&file);
    for (const QString &line : default_grub)
        stream << line << "\n";
    file.close();
}

int MainWindow::findMenuEntryById(const QString &id) const
{
    int count = 0;
    for (const QString &line : grub_cfg) {
        if (line.startsWith(QLatin1String("menuentry "))) {
            if (line.contains("--id " + id))
                return count;
            ++count;
        }
    }
    return -1;
}

QStringList MainWindow::getLinuxPartitions()
{
    const QStringList partitions = cmd.getCmdOut("lsblk -ln -o NAME,SIZE,FSTYPE,MOUNTPOINT,LABEL -e 2,11 -x NAME | "
        "grep -E '^x?[h,s,v].[a-z][0-9]|^mmcblk[0-9]+p|^nvme[0-9]+n[0-9]+p'").split(QStringLiteral("\n"), QString::SkipEmptyParts);
    QString part;
    QStringList new_list;
    for (const QString &part_info : partitions) {
        part = part_info.section(QStringLiteral(" "), 0, 0);
        if (cmd.run("lsblk -ln -o PARTTYPE /dev/" + part +
                "| grep -qEi '0x83|0fc63daf-8483-4772-8e79-3d69d8477de4|44479540-F297-41B2-9AF7-D131D5F0458A|4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709'"))
            new_list << part_info;
    }
    return new_list;
}

void MainWindow::cleanup()
{
    qDebug() << "Running MXBO cleanup code";
    if (!chroot.isEmpty()) {
        QString path = chroot.section(QStringLiteral(" "), 1, 1);
        if (path.isEmpty())
            return;
        // umount and clean temp folder
        cmd.run("mountpoint -q " + path + "/boot/efi && umount " + path + "/boot/efi");
        QString cmd_str = QStringLiteral("/bin/umount -R %1/run && /bin/umount -R %1/proc && /bin/umount -R %1/sys && /bin/umount -R %1/dev && umount %1 && rmdir %1").arg(path);
        cmd.run(cmd_str);
    }
}

QString MainWindow::selectPartiton(const QStringList &list)
{
    auto *dialog = new CustomDialog(list);

    // Guess MX install, find first partition with rootMX* label
    for (const QString &part_info : list) {
        if (cmd.run("lsblk -ln -o LABEL /dev/" + part_info.section(QStringLiteral(" "), 0 ,0) + "| grep -q rootMX")) {
            dialog->comboBox()->setCurrentIndex(dialog->comboBox()->findText(part_info));
            break;
        }
    }
    if (dialog->exec() == QDialog::Accepted) {
        qDebug() << "exec true" << dialog->comboBox()->currentText().section(QStringLiteral(" "), 0, 0);
        return dialog->comboBox()->currentText().section(QStringLiteral(" "), 0, 0);
    } else {
        qDebug() << "exec false" << dialog->comboBox()->currentText().section(QStringLiteral(" "), 0, 0);
        QMessageBox::critical(this, tr("Cannot continue"), tr("Nothing was selected, cannot change boot options. Exiting..."));
        exit(EXIT_FAILURE);
    }
}

// Add item to the key in /etc/default/grub
void MainWindow::addGrubArg(const QString &key, const QString &item)
{
    QStringList new_list;
    for (QString line : qAsConst(default_grub)) {
        if (line.contains(key)) {               // find key
            if (line.contains(item)) {          // return if already has the item
                return;
            } else if (line.endsWith(QLatin1String("="))) {    // empty line terminated in equal
                line.append(item);
            } else if (line.endsWith(QLatin1String("\"\""))) { // line that ends with a double quote
                line.chop(1); // chop last quote
                line.append(item).append("\"");
            } else if (line.endsWith(QLatin1String("\""))) {   // line ends with one quote (has other elements
                line.chop(1); // chop last quote
                line.append(" ").append(item).append("\"");
            } else if (line.endsWith(QLatin1String("''"))) {    // line ends with 2 single quotes
                line.chop(1); // chop last quote
                line.append(item).append("'");
            } else if     (line.endsWith(QLatin1String("'"))) {    // line ends with a single quote
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

void MainWindow::addGrubLine(const QString &item)
{
    default_grub << item;
}

void MainWindow::createChrootEnv(const QString& root)
{
    if (!tmpdir.isValid()) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not create a temporary folder"));
        exit(EXIT_FAILURE);
    }
    QString cmd_str = QStringLiteral("/bin/mount /dev/%1 %2 && /bin/mount --rbind --make-rslave /dev %2/dev && /bin/mount --rbind --make-rslave /sys %2/sys && /bin/mount --rbind /proc %2/proc && /bin/mount -t tmpfs -o size=100m,nodev,mode=755 tmpfs %2/run && /bin/mkdir %2/run/udev && /bin/mount --rbind /run/udev %2/run/udev").arg(root, tmpdir.path());
    if (!cmd.run(cmd_str)) {
        QMessageBox::critical(this, tr("Cannot continue"),
            tr("Cannot create chroot environment, cannot change boot options. Exiting..."));
        exit(EXIT_FAILURE);
    }
    chroot = "chroot " + tmpdir.path() + " ";
    ui->pushPreview->setDisabled(true); // no preview when running chroot.
}

// Uncomment or add line in /etc/default/grub
void MainWindow::enableGrubLine(const QString &item)
{
    bool found = false;
    QStringList new_list;
    for (const QString &line : qAsConst(default_grub)) {
        if (line == item || line.contains(QRegularExpression("^#.*" + item))) { // mark found and add item if found disabled or enabled
            found = true;
            new_list << item;
        } else {
            new_list << line;
        }
    }
    if (found)
        default_grub = new_list;
    else
        default_grub << QStringLiteral("\n") << item;
}

// Comment out line in /etc/default/grub that starts with passed item
void MainWindow::disableGrubLine(const QString &item)
{
    QStringList new_list;
    for (const QString &line : qAsConst(default_grub))
        if (line.startsWith(item))
            new_list << "#" + line;
        else
            new_list << line;
    default_grub = new_list;
}

// Remove itme from key in /etc/default/grub
void MainWindow::remGrubArg(const QString &key, const QString &item)
{
    QStringList new_list;
    new_list.reserve(default_grub.size());
    for (QString line : qAsConst(default_grub)) {
        if (line.contains(key))  // find key
            line.remove(QRegularExpression("\\s*" + item));
        new_list << line;
    }
    default_grub = new_list;
}

// Replace the argument in /etc/default/grub return false if nothing was replaced
bool MainWindow::replaceGrubArg(const QString &key, const QString &item)
{
    bool replaced = false;
    QStringList new_list;
    for (const QString &line : qAsConst(default_grub)) {
        if (line.contains(key)) { // find key
            new_list <<  key + "=" + item;
            replaced = true;
        } else {
            new_list << line;
        }
    }
    default_grub = new_list;
    return replaced;
}

void MainWindow::readGrubCfg()
{
    QFile file(chroot.section(QStringLiteral(" "), 1, 1) + "/boot/grub/grub.cfg");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    ui->comboMenuEntry->clear();

    int menu_level = 0;
    int menu_count = 0;
    int submenu_count = 0;
    QString menu_id;
    QString line;
    while (!file.atEnd()) {
        line = file.readLine().trimmed();
        grub_cfg << line;
        if (line.startsWith(QLatin1String("menuentry ")) || line.startsWith(QLatin1String("submenu "))) {
            menu_id = line.section(QStringLiteral("$menuentry_id_option"), 1, -1).section(QStringLiteral(" "), 1, 1);
            QString info;
            QString item = line.section(QRegularExpression(QStringLiteral("['\"]")), 1, 1);
            if (menu_level > 0) {
                info = menu_id + " " + QString::number(menu_count - 1) + ">" + QString::number(submenu_count);
                item = "    " + item;
                ++submenu_count;
            } else if (menu_level == 0) {
                info = menu_id + " " + QString::number(menu_count);
                ++menu_count;
            }
            ui->comboMenuEntry->addItem(item, info);
        }
        if (line.contains(QLatin1String("{")))  // assuming one "{" per line, this might not work in all cases and with custom made grub.cfg
            ++menu_level;
        if (line.contains(QLatin1String("}")))
            --menu_level;
        // reset submenu count
        if (menu_level == 0)
            submenu_count = 0;
    }
    file.close();
}

void MainWindow::readDefaultGrub()
{
    QFile file(chroot.section(QStringLiteral(" "), 1, 1) + "/etc/default/grub");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    QString line;
    while (!file.atEnd()) {
        line = file.readLine().trimmed();
        default_grub << line;
        if (line.startsWith(QLatin1String("GRUB_DEFAULT="))) {
            QString entry = line.section(QStringLiteral("="), 1, -1).remove(QStringLiteral("\"")).remove(QStringLiteral("'"));
            bool ok {false};
            int number = entry.toInt(&ok);
            if (ok) {
                if (ui->checkEnableFlatmenus->isChecked())
                    ui->comboMenuEntry->setCurrentIndex(number);
                else
                    ui->comboMenuEntry->setCurrentIndex(ui->comboMenuEntry->findData(" " + entry, Qt::UserRole, Qt::MatchEndsWith));
            } else if (entry == QLatin1String("saved")) {
                ui->checkSaveDefault->setChecked(true);
            } else if (entry.length() > 3) {  // if not saved but still long word assume it's a menuendtry_id or menuentry_name
                int index = ui->comboMenuEntry->findData(entry, Qt::UserRole, Qt::MatchContains);
                if (index != -1) // menuentry_id
                    ui->comboMenuEntry->setCurrentIndex(index);
                else             // menuentry_name most likely
                    ui->comboMenuEntry->setCurrentIndex(ui->comboMenuEntry->findText(entry));
            } else { // if 1>2 format
                int index = ui->comboMenuEntry->findData(entry, Qt::UserRole, Qt::MatchEndsWith);
                if (index != -1)
                    ui->comboMenuEntry->setCurrentIndex(index);
                else           // menuentry_name most likely
                    ui->comboMenuEntry->setCurrentIndex(ui->comboMenuEntry->findText(entry));
            }
        } else if (line.startsWith(QLatin1String("GRUB_TIMEOUT="))) {
            ui->spinBoxTimeout->setValue(line.section(QStringLiteral("="), 1, 1).remove(QStringLiteral("\"")).remove(QStringLiteral("'")).toInt());
        } else if (line.startsWith(QLatin1String("export GRUB_MENU_PICTURE="))) {
            ui->pushBgFile->setText(line.section(QStringLiteral("="), 1, 1).remove(QStringLiteral("\"")));
            ui->pushBgFile->setProperty("file", line.section(QStringLiteral("="), 1, 1).remove(QStringLiteral("\"")));
        } else if (line.startsWith(QLatin1String("GRUB_THEME="))) {
            ui->pushThemeFile->setText(line.section(QStringLiteral("="), 1, 1).remove(QStringLiteral("\"")));
            ui->pushThemeFile->setProperty("file", line.section(QStringLiteral("="), 1, 1).remove(QStringLiteral("\"")));
            if (QFile::exists(ui->pushThemeFile->property("file").toString())) {
                ui->pushThemeFile->setEnabled(true);
                ui->checkGrubTheme->setChecked(true);
                ui->pushBgFile->setDisabled(true);
            } else {
                ui->pushThemeFile->setDisabled(true);
                ui->pushBgFile->setEnabled(true);
                ui->pushThemeFile->setText(QLatin1String(""));
                ui->pushThemeFile->setProperty("file", "");
            }
        } else if (line.startsWith(QLatin1String("GRUB_CMDLINE_LINUX_DEFAULT="))) {
            ui->textKernel->setText(line.remove(QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT=")).remove(QStringLiteral("\"")).remove(QStringLiteral("'")));
            if (line.contains(QLatin1String("hush")))
                ui->radioLimitedMsg->setChecked(true);
            else if (line.contains(QLatin1String("quiet")))
                ui->radioDetailedMsg->setChecked(true);
            else
                ui->radioVeryDetailedMsg->setChecked(true);
            ui->checkBootsplash->setChecked(line.contains(QLatin1String("splash")));
            if (!isInstalled(QStringList() << QStringLiteral("plymouth") << QStringLiteral("plymouth-x11") << QStringLiteral("plymouth-themes") << QStringLiteral("plymouth-themes-mx")))
                ui->checkBootsplash->setChecked(false);
        } else if (line == QLatin1String("GRUB_DISABLE_SUBMENU=y")) {
            ui->checkEnableFlatmenus->setChecked(true);
        }
    }
    file.close();
}

// Read kernel line and options from /proc/cmdline
void MainWindow::readKernelOpts()
{
    QFile file(QStringLiteral("/proc/cmdline"));
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file.fileName();
        return;
    }
    kernel_options = file.readAll();
}

void MainWindow::cmdStart()
{
    setCursor(QCursor(Qt::BusyCursor));
    bar->setValue(0);
    timer.start(100ms);
}

void MainWindow::cmdDone()
{
    setCursor(QCursor(Qt::ArrowCursor));
    bar->setValue(bar->maximum());
    timer.stop();
}

void MainWindow::procTime()
{
    bar->setValue((bar->value() + 10) % bar->maximum());
}

void MainWindow::setConnections()
{
    // reset connection if calling a couple of times in a row
    timer.disconnect();
    timer.stop();

    connect(&timer, &QTimer::timeout, this, &MainWindow::procTime);
    connect(&cmd, &QProcess::started, this, &MainWindow::cmdStart);
    connect(&cmd, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::cmdDone);
}

void MainWindow::pushApply_clicked()
{
    ui->pushCancel->setDisabled(true);
    ui->pushApply->setDisabled(true);
    auto *progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);

    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(nullptr);
    progress->setWindowTitle(tr("Updating configuration, please wait"));
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->resize(500, progress->height());
    progress->show();
    setConnections();

    if (kernel_options_changed)
        replaceGrubArg(QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT"), "\"" + ui->textKernel->text() + "\"");

    if (options_changed) {
        cmd.run(QStringLiteral("grub-editenv /boot/grub/grubenv unset next_entry")); // uset the saved entry from grubenv
        if (ui->pushBgFile->isEnabled() && QFile::exists(ui->pushBgFile->property("file").toString())) {
            replaceGrubArg(QStringLiteral("export GRUB_MENU_PICTURE"), "\"" + ui->pushBgFile->property("file").toString() + "\"");
        } else if (ui->checkGrubTheme->isChecked() && QFile::exists(ui->pushThemeFile->property("file").toString())) {
            disableGrubLine(QStringLiteral("export GRUB_MENU_PICTURE"));
            if (!replaceGrubArg(QStringLiteral("GRUB_THEME"), "\"" + ui->pushThemeFile->property("file").toString() + "\""))
                addGrubLine("GRUB_THEME=\"" + ui->pushThemeFile->property("file").toString() + "\"");
        }
        if (ui->checkGrubTheme->isVisible() && !ui->checkGrubTheme->isChecked())
            disableGrubLine(QStringLiteral("GRUB_THEME="));

        // for simple menu index number is sufficient, if submenus exists use "1>1" format
        QString grub_entry = ui->checkEnableFlatmenus->isChecked()
                ? QString::number(ui->comboMenuEntry->currentIndex())
                : ui->comboMenuEntry->currentData().toString().section(QStringLiteral(" "), 1, 1);
        if (ui->comboMenuEntry->currentText().contains(QLatin1String("memtest"))) {
            ui->spinBoxTimeout->setValue(5);
            cmd.run(chroot + "grub-reboot \"" + ui->comboMenuEntry->currentText() + "\"");
        } else {
            replaceGrubArg(QStringLiteral("GRUB_DEFAULT"), "\"" + grub_entry + "\"");
        }
        if (ui->checkSaveDefault->isChecked()) {
            replaceGrubArg(QStringLiteral("GRUB_DEFAULT"), QStringLiteral("saved"));
            enableGrubLine(QStringLiteral("GRUB_SAVEDEFAULT=true"));
            cmd.run(chroot + "grub-set-default \"" + grub_entry + "\"");
        } else {
            disableGrubLine(QStringLiteral("GRUB_SAVEDEFAULT=true"));
        }
        replaceGrubArg(QStringLiteral("GRUB_TIMEOUT"), QString::number(ui->spinBoxTimeout->value()));
    }
    if (splash_changed) {
        if (ui->checkBootsplash->isChecked()) {
            if (!ui->comboTheme->currentText().isEmpty())
                cmd.run(chroot + "plymouth-set-default-theme " + ui->comboTheme->currentText());
            cmd.run(chroot + "update-rc.d bootlogd disable");
        } else {
            cmd.run(chroot + "update-rc.d bootlogd enable");
        }
        progress->setLabelText(tr("Updating initramfs..."));
        cmd.getCmdOut(chroot + "update-initramfs -u -k all");
    }
    if (messages_changed && ui->radioLimitedMsg->isChecked())
        cmd.run(chroot + "grep -q hush /etc/default/rcS || echo \"\n# hush boot-log into /run/rc.log\n"
                                 "[ \\\"\\$init\\\" ] && grep -qw hush /proc/cmdline && exec >> /run/rc.log 2>&1 || true \" >> /etc/default/rcS");
    if (options_changed || splash_changed || messages_changed) {
        if (grub_installed) {
            writeDefaultGrub();
            progress->setLabelText(tr("Updating grub..."));
            cmd.getCmdOut(chroot + "update-grub");
        }
        progress->close();
        QMessageBox::information(this, tr("Done") , tr("Changes have been successfully applied."));
    }
    options_changed = false;
    splash_changed = false;
    messages_changed = false;
    ui->pushCancel->setEnabled(true);
}

void MainWindow::pushAbout_clicked()
{
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About") + " MX Boot Options",
                       R"(<p align="center"><b><h2>MX Boot Options</h2></b></p><p align="center">)" +
                       tr("Version: ") + qApp->applicationVersion() + "</p><p align=\"center\"><h3>" +
                       tr("Program for selecting common start-up choices") +
                       R"(</h3></p><p align="center"><a href="http://mxlinux.org">http://mxlinux.org</a><br /></p><p align="center">)" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    auto *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    auto *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    auto *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        qputenv("HOME", starting_home.toUtf8());
        const QString url = QStringLiteral("file:///usr/share/doc/mx-boot-options/license.html");
        if (system("command -v mx-viewer >/dev/null") == 0)
            system("mx-viewer " + url.toUtf8() + " " + tr("License").toUtf8() + "&");
        else
            system("runuser " + user.toUtf8() + " -c \"env XDG_RUNTIME_DIR=/run/user/$(id -u " +
                   user.toUtf8() + ") xdg-open " + url.toUtf8() + "\"&");
        qputenv("HOME", "/root");
    } else if (msgBox.clickedButton() == btnChangelog) {
        auto *changelog = new QDialog(this);
        changelog->setWindowTitle(tr("Changelog"));
        changelog->resize(600, 500);

        auto *text = new QTextEdit;
        text->setReadOnly(true);

        QProcess proc;
        proc.start(QStringLiteral("zless"), QStringList{"/usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName() +
                                        "/changelog.gz"}, QIODevice::ReadOnly);
        proc.waitForFinished();
        text->setText(proc.readAllStandardOutput());

        auto *btnClose = new QPushButton(tr("Close"));
        btnClose->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        auto *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
}

void MainWindow::pushHelp_clicked()
{
    const QString url = QStringLiteral("/usr/share/doc/mx-boot-options/mx-boot-options.html");

    qputenv("HOME", starting_home.toUtf8());
    if (system("command -v mx-viewer >/dev/null") == 0)
        system("mx-viewer " + url.toUtf8() + " \"" + tr("MX Boot Options").toUtf8() + "\"&");
    else
        system("runuser " + user.toUtf8() + " -c \"env XDG_RUNTIME_DIR=/run/user/$(id -u " +
               user.toUtf8() + ") xdg-open " + url.toUtf8() + "\"&");
    qputenv("HOME", "/root");
}

void MainWindow::combo_bootsplash_clicked(bool checked)
{
    ui->radioLimitedMsg->setVisible(!checked);
    if (checked) {
        if (inVirtualMachine()) {
            QMessageBox::information(this, tr("Running in a Virtual Machine"),
                                     tr("You current system is running in a Virtual Machine,\n"
                                        "Plymouth bootsplash will work in a limited way, you also won't be able to preview the theme"));
            ui->pushPreview->setDisabled(true);
        }
        if (!isInstalled(QStringList() << QStringLiteral("plymouth") << QStringLiteral("plymouth-x11") << QStringLiteral("plymouth-themes") << QStringLiteral("plymouth-themes-mx"))) {
            int ans = QMessageBox::question(this, tr("Plymouth packages not installed"), tr("Plymouth packages are not currently installed.\nOK to go ahead and install them?"));
            if (ans == QMessageBox::No) {
                ui->checkBootsplash->setChecked(false);
                ui->radioLimitedMsg->setVisible(!checked);
                return;
            }
            installSplash();
            just_installed = true;
        }
        loadPlymouthThemes();
        if (ui->radioLimitedMsg->isChecked())
            ui->radioDetailedMsg->setChecked(true);
    }
    splash_changed = true;
    ui->pushApply->setEnabled(true);
}

void MainWindow::btn_bg_file_clicked()
{
    QString selected = QFileDialog::getOpenFileName(this, QObject::tr("Select image to display in bootloader"),
                                                    chroot.section(QStringLiteral(" "), 1, 1) + "/usr/share/backgrounds/MXLinux/grub",
                                                    tr("Images (*.png *.jpg *.jpeg *.tga)"));
    if (!selected.isEmpty()) {
        if (!chroot.isEmpty())
            selected.remove(chroot.section(QStringLiteral(" "), 1, 1));
        ui->pushBgFile->setText(selected);
        ui->pushBgFile->setProperty("file", selected);
        options_changed = true;
        ui->pushApply->setEnabled(true);
    }
}

void MainWindow::radio_detailed_msg_toggled(bool checked)
{
    if (checked) {
        messages_changed = true;
        ui->pushApply->setEnabled(true);
        QString line = ui->textKernel->text();
        if (!line.contains(QLatin1String("quiet"))) {
            if (!line.isEmpty())
                line.append(" ");
            line.append("quiet");
        }
        line.remove(QRegularExpression(QStringLiteral("\\s*hush")));
        ui->textKernel->setText(line.trimmed());
    }
}

void MainWindow::radio_very_detailed_msg_toggled(bool checked)
{
    if (checked) {
        messages_changed = true;
        ui->pushApply->setEnabled(true);
        QString line = ui->textKernel->text();
        line.remove(QRegularExpression(QStringLiteral("\\s*quiet")));
        line.remove(QRegularExpression(QStringLiteral("\\s*hush")));
        ui->textKernel->setText(line.trimmed());
    }
}

void MainWindow::radio_limited_msg_toggled(bool checked)
{
    if (checked) {
        messages_changed = true;
        ui->pushApply->setEnabled(true);
        QString line = ui->textKernel->text();
        if (!line.contains(QLatin1String("quiet"))) {
            if (!line.isEmpty())
                line.append(" ");
            line.append("quiet");
        }
        if (!line.contains(QLatin1String("hush"))) {
            if (!line.endsWith(QLatin1String(" ")))
                line.append(" ");
            line.append("hush");
        }
        ui->textKernel->setText(line);
    }
}

void MainWindow::spinBoxTimeout_valueChanged(int /*unused*/)
{
    options_changed = true;
    ui->pushApply->setEnabled(true);
}

void MainWindow::combo_menu_entry_currentIndexChanged()
{
    options_changed = true;
    ui->pushApply->setEnabled(true);
}

// Toggled either by user or when reading the status of bootsplash
void MainWindow::combo_bootsplash_toggled(bool checked)
{
    ui->comboTheme->setEnabled(checked);
    ui->pushPreview->setEnabled(checked);
    loadPlymouthThemes();
    QString line = ui->textKernel->text();
    if (checked) {
        if (!line.contains(QLatin1String("splash"))) {
            if (!line.isEmpty())
                line.append(" ");
            line.append("splash");
        }
    } else {
        line.remove(QRegularExpression(QStringLiteral("\\s*splash")));
    }
    ui->textKernel->setText(line.trimmed());
}

void MainWindow::pushLog_clicked()
{
    QString location = QStringLiteral("/var/log/boot.log");
    if (kernel_options.contains(QLatin1String("hush")))
        location = QStringLiteral("/run/rc.log");
    QString sed = QStringLiteral(R"(sed 's/\^\[/\x1b/g')");  // remove formatting escape char
    if (!QFile::exists(location)) // try aternate location
        location = QStringLiteral("/var/log/boot");

    qputenv("HOME", starting_home.toUtf8());
    if (QFile::exists(location))
        cmd.run("x-terminal-emulator -e bash -c \"" + sed + " " + location + "; read -n1 -srp '"+ tr("Press any key to close") + "'\"&");
    else
        QMessageBox::critical(this, tr("Log not found"), tr("Could not find log at ") + location);
    qputenv("HOME", "/root");
}


void MainWindow::combo_theme_activated(int /*unused*/)
{
    splash_changed = true;
    ui->pushApply->setEnabled(true);
}

void MainWindow::push_preview_clicked()
{
    if (just_installed)
        QMessageBox::warning(this, tr("Needs reboot"), tr("Plymouth was just installed, you might need to reboot before being able to display previews"));
    QString current_theme = cmd.getCmdOut(QStringLiteral("plymouth-set-default-theme"));
    if (ui->comboTheme->currentText() == QLatin1String("details"))
        return;
    qputenv("HOME", starting_home.toUtf8());
    cmd.getCmdOut("plymouth-set-default-theme " + ui->comboTheme->currentText());
    ////connect(cmd, &Cmd::runTime, this, &MainWindow::sendMouseEvents);
    cmd.getCmdOut(QStringLiteral("x-terminal-emulator -e bash -c 'plymouthd; plymouth --show-splash; for ((i=0; i<4; i++)); do plymouth --update=test$i; sleep 1; done; plymouth quit'"));
    cmd.getCmdOut("plymouth-set-default-theme " + current_theme); // return to current theme
    cmd.disconnect();
    qputenv("HOME", "/root");
}

void MainWindow::combo_enable_flatmenus_clicked(bool checked)
{
    auto *progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);

    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(nullptr);
    progress->setWindowTitle(tr("Updating configuration, please wait"));
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->resize(500, progress->height());
    progress->show();

    if (checked)
        enableGrubLine(QStringLiteral("GRUB_DISABLE_SUBMENU=y"));
    else
        disableGrubLine(QStringLiteral("GRUB_DISABLE_SUBMENU=y"));

    writeDefaultGrub();
    progress->setLabelText(tr("Updating grub..."));
    setConnections();
    cmd.getCmdOut(chroot + "update-grub");
    readGrubCfg();
    progress->close();
}

void MainWindow::combo_save_default_clicked()
{
    options_changed = true;
    ui->pushApply->setEnabled(true);
}

void MainWindow::combo_theme_currentIndexChanged(int index)
{
    ui->pushPreview->setDisabled(ui->comboTheme->itemText(index) == QLatin1String("details"));
}

void MainWindow::combo_grub_theme_toggled(bool checked)
{
    if (checked && ui->pushThemeFile->property("file").toString().isEmpty()) {
        ui->pushThemeFile->setText(tr("Click to select theme"));
        ui->pushThemeFile->setProperty("file", "");
    } else {
        options_changed = true;
        ui->pushApply->setEnabled(true);
    }
}

void MainWindow::btn_theme_file_clicked()
{
    QString selected = QFileDialog::getOpenFileName(this, QObject::tr("Select GRUB theme"),
                                              chroot.section(QStringLiteral(" "), 1, 1) + "/boot/grub/themes", QStringLiteral("*.txt;; *.*"));
    if (!selected.isEmpty()) {
        if (!chroot.isEmpty())
            selected.remove(chroot.section(QStringLiteral(" "), 1, 1));
        ui->pushThemeFile->setText(selected);
        ui->pushThemeFile->setProperty("file", selected);
        options_changed = true;
        ui->pushApply->setEnabled(true);
    }
}

void MainWindow::lineEdit_kernel_textEdited()
{
    kernel_options_changed = true;
    options_changed = true;
    ui->pushApply->setEnabled(true);
}
