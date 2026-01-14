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
#include <QProcess>
#include <QSize>
#include <cstdio>

// libcurl
extern "C" {
#include <curl/curl.h>
}

// Callback для записи данных из FTP в std::string
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

    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->setMinimumSize(this->size());
    this->setMaximumSize(this->size());
    this->setFixedSize(QSize(795, 549));
    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);
    networkManager = new QNetworkAccessManager(this);

    ftpModel = new QStandardItemModel(this);
    ui->treeView->setModel(ftpModel);
    ftpModel->setHorizontalHeaderLabels({"Подключитесь к FTP-серверу"});

    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::on_tcpSocket_readyRead);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::on_tcpSocket_errorOccurred);

    connect(ui->treeView, &QTreeView::doubleClicked, this, &MainWindow::on_treeView_doubleClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ===== TCP =====
void MainWindow::on_pushButton_5_clicked()
{
    QString input = ui->lineEdit_3->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите адрес сервера");
        return;
    }

    QString host = input;
    int port = 80;
    if (input.contains(':')) {
        QStringList parts = input.split(':');
        host = parts[0];
        bool ok;
        int p = parts[1].toInt(&ok);
        if (ok && p > 0 && p < 65536) port = p;
    }

    ui->listWidget_2->clear();
    ui->listWidget_2->addItem(QString("Подключение к %1:%2...").arg(host).arg(port));
    tcpSocket->connectToHost(host, port);
}

void MainWindow::on_tcpSocket_readyRead()
{
    QByteArray data = tcpSocket->readAll();
    ui->listWidget_2->addItem("Ответ: " + QString::fromUtf8(data));
}

void MainWindow::on_tcpSocket_errorOccurred(QAbstractSocket::SocketError)
{
    ui->listWidget_2->addItem("TCP ошибка: " + tcpSocket->errorString());
}

// ===== UDP =====
void MainWindow::on_pushButton_4_clicked()
{
    QString input = ui->lineEdit_2->text().trimmed();
    if (input.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите адрес сервера");
        return;
    }

    QString host = input;
    int port = 80;
    if (input.contains(':')) {
        QStringList parts = input.split(':');
        host = parts[0];
        bool ok;
        int p = parts[1].toInt(&ok);
        if (ok && p > 0 && p < 65536) port = p;
    }

    QByteArray message = "Hello from UDP client";
    udpSocket->writeDatagram(message, QHostAddress(host), port);
    ui->listWidget->addItem(QString("Отправлено на %1:%2").arg(host).arg(port));
}

void MainWindow::on_udpSocket_readyRead()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(udpSocket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        ui->listWidget->addItem(QString("От %1:%2 → %3")
                                    .arg(sender.toString())
                                    .arg(senderPort)
                                    .arg(QString::fromUtf8(datagram)));
    }
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

    // Диалог аутентификации
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

// ===== FTP: загрузка директории =====
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
                    // Имя файла — последнее непустое слово
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

// ===== FTP: двойной клик по элементу =====
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

// ===== FTP: скачать выбранный файл =====
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
        // === Скачивание файла (как раньше) ===
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
        // === Скачивание папки как ZIP ===
        QString folderName = itemName;
        QString zipName = folderName + ".zip";
        QString savePath = QFileDialog::getSaveFileName(this, "Сохранить папку как ZIP", zipName, "ZIP архивы (*.zip)");
        if (savePath.isEmpty()) return;

        // Создаём временную директорию
        QDir tempDir(QDir::temp().filePath("ftp_download_" + QString::number(QDateTime::currentMSecsSinceEpoch())));
        if (!tempDir.mkpath(".")) {
            QMessageBox::critical(this, "Ошибка", "Не удалось создать временную папку");
            return;
        }

        // Формируем URL папки
        QString folderUrl = QString("ftp://%1%2%3/")
                                .arg(currentFtpHost)
                                .arg(currentFtpPath.endsWith('/') ? currentFtpPath : currentFtpPath + "/")
                                .arg(folderName);

        // Скачиваем содержимое папки (просто текстовый список)
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
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }

        QStringList lines = QString::fromStdString(response).split('\n');
        bool hasFiles = false;

        // Скачиваем каждый файл в папку
        for (const QString &line : qAsConst(lines)) {
            QString s = line.trimmed();
            if (s.isEmpty() || s[0] != '-') continue; // только файлы

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

        // Запускаем `zip -r archive.zip .` внутри временной папки
        QProcess zipProc;
        zipProc.setWorkingDirectory(tempDir.path());
        zipProc.start("zip", QStringList() << "-r" << QDir::toNativeSeparators(QFileInfo(savePath).absoluteFilePath()) << ".");
        zipProc.waitForFinished(-1);

        if (zipProc.exitCode() != 0) {
            QMessageBox::critical(this, "Ошибка ZIP",
                                  "Не удалось создать архив. Убедитесь, что установлен пакет 'zip'.\n\n" +
                                      QString::fromLocal8Bit(zipProc.readAllStandardError()));
        } else {
            QMessageBox::information(this, "Успех", "Папка сохранена как ZIP-архив!");
        }

        // Удаляем временные файлы
        tempDir.removeRecursively();
    }
}

// ===== FTP: отключиться =====
void MainWindow::on_pushButton_3_clicked()
{
    currentFtpHost.clear();
    currentFtpPath = "/";
    currentFtpUsername.clear();
    currentFtpPassword.clear();
    ftpModel->clear();
    ftpModel->setHorizontalHeaderLabels({"Отключено"});
}
