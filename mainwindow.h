#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QStandardItemModel>
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // TCP
    void on_pushButton_5_clicked();
    void on_tcpSocket_readyRead();
    void on_tcpSocket_errorOccurred(QAbstractSocket::SocketError);

    // UDP
    void on_pushButton_4_clicked();
    void on_udpSocket_readyRead();

    // FTP
    void on_pushButton_clicked();   // Подключиться
    void on_pushButton_3_clicked(); // Отключиться
    void on_pushButton_2_clicked(); // Скачать

    void on_treeView_doubleClicked(const QModelIndex &index);

private:
    void loadFtpDirectory(const QString &path);

    Ui::MainWindow *ui;
    QTcpSocket *tcpSocket;
    QUdpSocket *udpSocket;
    QStandardItemModel *ftpModel;
    QNetworkAccessManager *networkManager;

    // FTP state
    QString currentFtpHost;
    QString currentFtpPath;
    QString currentFtpUsername;
    QString currentFtpPassword;
};

#endif // MAINWINDOW_H
