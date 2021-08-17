#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <QMetaType>

using Port = quint16;

// After new client connected, a thread will be created.
// Demo only
template <typename ClientHandle>
class SocketServer : public QTcpServer
{
public:
    SocketServer(QObject *parent = nullptr)
        :QTcpServer(parent) {
        qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    }
    ~SocketServer() { uninit(); }

public:
    bool init(const QString &ip_addr, const Port &port) {
        QHostAddress host_addr(ip_addr);
        return this->listen(host_addr, port);
    }
    bool uninit() {
        if(this->isListening()) this->close();

        for(auto iter = connections_.begin();
            iter != connections_.end();)
        {
            delete (*iter);
            iter = connections_.erase(iter);
        }

        return true;
    }
    bool sendToAll(const QByteArray &data){
        for(auto iter = connections_.begin();
            iter != connections_.end();){

            if((*iter)->state() == QAbstractSocket::UnconnectedState){
                iter = connections_.erase(iter);
                continue;
            }
            else if(!(*iter)->isValid()){
                continue;
            }

            (*iter)->send(data);
            ++iter;
        }
        return true;
    }

private:
    void incomingConnection(qintptr handle) override {
        QThread *thread = new QThread();

        ClientHandle *tcp_client =  new ClientHandle(handle, thread);
        connections_.push_back(tcp_client);
    }

private:
    std::list<ClientHandle*> connections_;
};

class ClientHandle : public QTcpSocket{
    Q_OBJECT

public:
    ClientHandle(qintptr handle, QThread *thread) {
        this->setSocketDescriptor(handle);

        connect(this, &ClientHandle::readyRead,
                this, &ClientHandle::onReadyRead);
        connect(this, &ClientHandle::sendData,
                this, &ClientHandle::onSendData);
        connect(this, &ClientHandle::disconnected,
                thread, &QThread::quit);

        this->moveToThread(thread);
        thread->start();
    }
    ~ClientHandle() { this->disconnect(); }

    void send(const QByteArray &data){
        emit sendData(data);
    }


protected:
    virtual void onHandleData(const QByteArray &, QByteArray &){ }

signals:
    void sendData(const QByteArray &data);

private slots:
    void onSendData(const QByteArray &data){
        this->write(data);
    }
    void onReadyRead(){
        ClientHandle *socket = (ClientHandle*)sender();

        QByteArray recv_bytes, send_bytes;
        recv_bytes = socket->readAll();

        onHandleData(recv_bytes, send_bytes);

        if(send_bytes.size() == 0) return;

        if(socket->write(send_bytes) == -1)
            qDebug()<< "Write failed.";

        return;
    }
    void onSocketError(QAbstractSocket::SocketError) {
        ClientHandle* socket = (ClientHandle*)sender();
        socket->disconnect();

        return;
    }

};
#endif // SOCKETSERVER_H
