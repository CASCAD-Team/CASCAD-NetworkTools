#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QUrl>
#include <QHostAddress>
#include <QStandardItem>
#include <QApplication>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QDateTime>
#include <QProcess>
#include <cstdio>

// libcurl
extern "C" {
#include <curl/curl.h>
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , currentFtpPath("/")
{
    ui->setupUi(this);

    // Фиксированный размер окна
    this->setFixedSize(this->size());

    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);
    networkManager = new QNetworkAccessManager(this);

    ftpModel = new QStandardItemModel(this);
    ui->treeView->setModel(ftpModel);
    ftpModel->setHorizontalHeaderLabels({"Подключитесь к FTP-серверу"});
    ui->treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::on_tcpSocket_readyRead);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::on_tcpSocket_errorOccurred);

    connect(ui->treeView, &QTreeView::doubleClicked, this, &MainWindow::on_treeView_doubleClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ===== TCP: отправить запрос =====
void MainWindow::on_pushButton_sendTcp_clicked()
{
    QString host = ui->lineEdit_tcpHost->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хост");
        return;
    }
    int port = 80;
    ui->listWidget_tcp->clear();
    ui->listWidget_tcp->addItem(QString("Подключение к %1:%2...").arg(host).arg(port));
    tcpSocket->connectToHost(host, port);
}

void MainWindow::on_tcpSocket_readyRead()
{
    QByteArray data = tcpSocket->readAll();
    ui->listWidget_tcp->addItem("Ответ: " + QString::fromUtf8(data));
}

void MainWindow::on_tcpSocket_errorOccurred(QAbstractSocket::SocketError)
{
    ui->listWidget_tcp->addItem("TCP ошибка: " + tcpSocket->errorString());
}

// ===== TCP: сканирование портов =====
QList<int> MainWindow::parsePortRange(const QString &range)
{
    QList<int> ports;
    QStringList parts = range.split(',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.contains('-')) {
            QStringList bounds = part.split('-');
            if (bounds.size() == 2) {
                bool ok1, ok2;
                int start = bounds[0].toInt(&ok1);
                int end = bounds[1].toInt(&ok2);
                if (ok1 && ok2 && start > 0 && end <= 65535 && start <= end) {
                    for (int p = start; p <= end; ++p)
                        ports << p;
                }
            }
        } else {
            bool ok;
            int port = part.toInt(&ok);
            if (ok && port > 0 && port <= 65535)
                ports << port;
        }
    }
    return ports;
}

void MainWindow::on_pushButton_scanTcp_clicked()
{
    QString host = ui->lineEdit_tcpHost->text().trimmed();
    QString portRange = ui->lineEdit_tcpPorts->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хост");
        return;
    }
    if (portRange.isEmpty()) portRange = "1-1000";

    ui->listWidget_tcp->clear();
    ui->listWidget_tcp->addItem(QString("Сканирование TCP портов на %1...").arg(host));

    QList<int> ports = parsePortRange(portRange);
    for (int port : ports) {
        QTcpSocket socket;
        socket.connectToHost(host, port);
        if (socket.waitForConnected(800)) {
            ui->listWidget_tcp->addItem(QString("✅ Открыт: %1/tcp").arg(port));
            socket.disconnectFromHost();
        }
        QApplication::processEvents();
    }
    ui->listWidget_tcp->addItem("Сканирование завершено.");
}

// ===== UDP: отправить датаграмму =====
void MainWindow::on_pushButton_sendUdp_clicked()
{
    QString host = ui->lineEdit_udpHost->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хост");
        return;
    }
    int port = 80;
    QByteArray msg = "Hello from UDP client";
    udpSocket->writeDatagram(msg, QHostAddress(host), port);
    ui->listWidget_udp->addItem(QString("📤 Отправлено на %1:%2").arg(host).arg(port));
}

void MainWindow::on_udpSocket_readyRead()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(udpSocket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        ui->listWidget_udp->addItem(QString("📥 От %1:%2 → %3")
                                        .arg(sender.toString())
                                        .arg(senderPort)
                                        .arg(QString::fromUtf8(datagram)));
    }
}

// ===== UDP: сканирование портов =====
void MainWindow::on_pushButton_scanUdp_clicked()
{
    QString host = ui->lineEdit_udpHost->text().trimmed();
    QString portRange = ui->lineEdit_udpPorts->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хост");
        return;
    }
    if (portRange.isEmpty()) portRange = "53,67,68,123,161";

    ui->listWidget_udp->clear();
    ui->listWidget_udp->addItem(QString("Сканирование UDP портов на %1...").arg(host));
    ui->listWidget_udp->addItem("⚠️ UDP-скан может быть неточным!");

    QList<int> ports = parsePortRange(portRange);
    QUdpSocket socket;
    QHostAddress addr(host);

    for (int port : ports) {
        socket.writeDatagram("", addr, port);
        if (socket.waitForReadyRead(500)) {
            while (socket.hasPendingDatagrams()) {
                QByteArray datagram;
                datagram.resize(static_cast<int>(socket.pendingDatagramSize()));
                socket.readDatagram(datagram.data(), datagram.size());
                ui->listWidget_udp->addItem(QString("📥 Ответ на %1/udp").arg(port));
            }
        }
        QApplication::processEvents();
    }
    ui->listWidget_udp->addItem("Сканирование завершено.");
}

// ===== FTP: подключение =====
void MainWindow::on_pushButton_clicked()
{
    QString hostInput = ui->lineEdit->text().trimmed();
    if (hostInput.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите адрес FTP-сервера");
        return;
    }

    QUrl url(hostInput.startsWith("ftp://") ? hostInput : "ftp://" + hostInput);
    if (!url.isValid() || url.host().isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Некорректный адрес FTP-сервера");
        return;
    }

    currentFtpHost = url.host();
    currentFtpPath = "/";

    QDialog authDialog(this);
    authDialog.setWindowTitle("Аутентификация FTP");
    QVBoxLayout mainLayout(&authDialog);

    QLabel *infoLabel = new QLabel("Для анонимного доступа:\nЛогин: anonymous\nПароль: можно оставить пустым");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #aaa; font-size: 11px;");

    QFormLayout formLayout;
    QLineEdit loginEdit;
    QLineEdit passEdit;
    passEdit.setEchoMode(QLineEdit::Password);
    loginEdit.setText("anonymous");

    formLayout.addRow("Логин:", &loginEdit);
    formLayout.addRow("Пароль:", &passEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &authDialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &authDialog, &QDialog::reject);

    mainLayout.addWidget(infoLabel);
    mainLayout.addLayout(&formLayout);
    mainLayout.addWidget(&buttonBox);

    if (authDialog.exec() != QDialog::Accepted) {
        return;
    }

    currentFtpUsername = loginEdit.text();
    currentFtpPassword = passEdit.text();

    loadFtpDirectory(currentFtpPath);
}

void MainWindow::loadFtpDirectory(const QString &path)
{
    QString fullUrl = QString("ftp://%1%2").arg(currentFtpHost).arg(path);
    if (!path.endsWith('/')) fullUrl += '/';

    ftpModel->clear();
    ftpModel->setHorizontalHeaderLabels({"Загрузка..."});
    QApplication::processEvents();

    CURL *curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.toStdString().c_str());
        if (!currentFtpUsername.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, currentFtpUsername.toStdString().c_str());
        }
        if (!currentFtpPassword.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_PASSWORD, currentFtpPassword.toStdString().c_str());
        }
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        ftpModel->clear();

        if (res != CURLE_OK) {
            QString error = QString("FTP ошибка: %1").arg(curl_easy_strerror(res));
            QMessageBox::critical(this, "Ошибка", error);
            ftpModel->setHorizontalHeaderLabels({error});
        } else {
            ftpModel->setHorizontalHeaderLabels({"Файлы и папки"});
            currentFtpPath = path;
            QStringList lines = QString::fromStdString(response).split('\n');
            for (const QString &line : qAsConst(lines)) {
                QString s = line.trimmed();
                if (s.isEmpty()) continue;

                QStandardItem *item = new QStandardItem(s);

                if (s.length() >= 1) {
                    QChar first = s[0];
                    if (first == 'd') {
                        item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
                        item->setData("dir", Qt::UserRole);
                    } else if (first == '-') {
                        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
                        item->setData("file", Qt::UserRole);
                    }
                    QStringList parts = s.split(' ', Qt::SkipEmptyParts);
                    if (!parts.isEmpty()) {
                        QString filename = parts.last();
                        item->setData(filename, Qt::UserRole + 1);
                    }
                }

                ftpModel->appendRow(item);
            }
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось инициализировать libcurl");
        ftpModel->clear();
        ftpModel->setHorizontalHeaderLabels({"Ошибка libcurl"});
    }
}

void MainWindow::on_treeView_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QStandardItem *item = ftpModel->itemFromIndex(index);
    if (!item) return;

    QString type = item->data(Qt::UserRole).toString();
    QString filename = item->data(Qt::UserRole + 1).toString();

    if (type == "dir") {
        QString newPath = currentFtpPath;
        if (!newPath.endsWith('/')) newPath += '/';
        newPath += filename;
        loadFtpDirectory(newPath);
    }
}

void MainWindow::on_pushButton_2_clicked()
{
    QModelIndexList selected = ui->treeView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "Скачать", "Выберите файл или папку.");
        return;
    }

    QModelIndex index = selected.first();
    QStandardItem *item = ftpModel->itemFromIndex(index);
    if (!item) return;

    QString type = item->data(Qt::UserRole).toString();
    QString itemName = item->data(Qt::UserRole + 1).toString();

    if (type == "file") {
        QString savePath = QFileDialog::getSaveFileName(this, "Сохранить файл как", itemName);
        if (savePath.isEmpty()) return;

        QString fileUrl = QString("ftp://%1%2%3")
                              .arg(currentFtpHost)
                              .arg(currentFtpPath.endsWith('/') ? currentFtpPath : currentFtpPath + "/")
                              .arg(itemName);

        FILE *fp = fopen(savePath.toStdString().c_str(), "wb");
        if (!fp) {
            QMessageBox::critical(this, "Ошибка", "Не удалось создать файл.");
            return;
        }

        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, fileUrl.toStdString().c_str());
            if (!currentFtpUsername.isEmpty()) {
                curl_easy_setopt(curl, CURLOPT_USERNAME, currentFtpUsername.toStdString().c_str());
            }
            if (!currentFtpPassword.isEmpty()) {
                curl_easy_setopt(curl, CURLOPT_PASSWORD, currentFtpPassword.toStdString().c_str());
            }
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            fclose(fp);

            if (res != CURLE_OK) {
                QFile::remove(savePath);
                QMessageBox::critical(this, "Ошибка", QString("Не удалось скачать:\n%1").arg(curl_easy_strerror(res)));
            } else {
                QMessageBox::information(this, "Успех", "Файл скачан!");
            }
        } else {
            fclose(fp);
            QFile::remove(savePath);
            QMessageBox::critical(this, "Ошибка", "libcurl не инициализирован");
        }

    } else if (type == "dir") {
        QString folderName = itemName;
        QString zipName = folderName + ".zip";
        QString savePath = QFileDialog::getSaveFileName(this, "Сохранить папку как ZIP", zipName, "ZIP архивы (*.zip)");
        if (savePath.isEmpty()) return;

        QDir tempDir(QDir::temp().filePath("ftp_download_" + QString::number(QDateTime::currentMSecsSinceEpoch())));
        if (!tempDir.mkpath(".")) {
            QMessageBox::critical(this, "Ошибка", "Не удалось создать временную папку");
            return;
        }

        QString folderUrl = QString("ftp://%1%2%3/")
                                .arg(currentFtpHost)
                                .arg(currentFtpPath.endsWith('/') ? currentFtpPath : currentFtpPath + "/")
                                .arg(folderName);

        CURL *curl = curl_easy_init();
        std::string response;
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, folderUrl.toStdString().c_str());
            if (!currentFtpUsername.isEmpty()) {
                curl_easy_setopt(curl, CURLOPT_USERNAME, currentFtpUsername.toStdString().c_str());
            }
            if (!currentFtpPassword.isEmpty()) {
                curl_easy_setopt(curl, CURLOPT_PASSWORD, currentFtpPassword.toStdString().c_str());
            }
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }

        QStringList lines = QString::fromStdString(response).split('\n');
        bool hasFiles = false;

        for (const QString &line : qAsConst(lines)) {
            QString s = line.trimmed();
            if (s.isEmpty() || s[0] != '-') continue;

            QStringList parts = s.split(' ', Qt::SkipEmptyParts);
            if (parts.isEmpty()) continue;
            QString filename = parts.last();
            if (filename == "." || filename == "..") continue;

            hasFiles = true;
            QString fileUrl = folderUrl + filename;
            QString localPath = tempDir.filePath(filename);

            FILE *fp = fopen(localPath.toStdString().c_str(), "wb");
            if (!fp) continue;

            CURL *c = curl_easy_init();
            if (c) {
                curl_easy_setopt(c, CURLOPT_URL, fileUrl.toStdString().c_str());
                if (!currentFtpUsername.isEmpty()) {
                    curl_easy_setopt(c, CURLOPT_USERNAME, currentFtpUsername.toStdString().c_str());
                }
                if (!currentFtpPassword.isEmpty()) {
                    curl_easy_setopt(c, CURLOPT_PASSWORD, currentFtpPassword.toStdString().c_str());
                }
                curl_easy_setopt(c, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, nullptr);
                curl_easy_perform(c);
                curl_easy_cleanup(c);
            }
            fclose(fp);
        }

        if (!hasFiles) {
            tempDir.removeRecursively();
            QMessageBox::warning(this, "Пустая папка", "Папка пуста или содержит только подкаталоги.");
            return;
        }

        QProcess zipProc;
        zipProc.setWorkingDirectory(tempDir.path());
        zipProc.start("zip", QStringList() << "-r" << QDir::toNativeSeparators(QFileInfo(savePath).absoluteFilePath()) << ".");
        zipProc.waitForFinished(-1);

        if (zipProc.exitCode() != 0) {
            QMessageBox::critical(this, "Ошибка ZIP",
                                  "Не удалось создать архив. Установите 'zip': sudo pacman -S zip");
        } else {
            QMessageBox::information(this, "Успех", "Папка сохранена как ZIP-архив!");
        }

        tempDir.removeRecursively();
    }
}

void MainWindow::on_pushButton_3_clicked()
{
    currentFtpHost.clear();
    currentFtpPath = "/";
    currentFtpUsername.clear();
    currentFtpPassword.clear();
    ftpModel->clear();
    ftpModel->setHorizontalHeaderLabels({"Отключено"});
}
