#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QtDBus>
#include <QTimer>
#include "console.h"

typedef QMap<QString,QVariantMap> InterfacesMap;
typedef QList<QString> InterfacesList;
typedef QMap<QDBusObjectPath,InterfacesMap> ObjectsMap;

Q_DECLARE_METATYPE(InterfacesMap);
Q_DECLARE_METATYPE(ObjectsMap);

class SerialPortManager : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager();

    Q_INVOKABLE void writeMessage(QString msg);

signals:
    void rfcommPortAvailable();
    void messageReceived(QString msg);

private slots:
    void registerProfileSPP(QDBusInterface *manager);
    void fetchObjects(QDBusInterface *manager);
    void registerDevice(QDBusObjectPath path);
    void onInterfacesAdded(const QDBusObjectPath &path, const InterfacesMap interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const InterfacesMap interfaces);
    void onPropertiesChanged(QString interface, QVariantMap changed_properties, QStringList invalidated_properties);
    void checkPortAvailable();
    void connectSerialPort();
    void readMessage();

private:
    QSerialPort *m_serialPort;
    QDBusInterface *m_blueManager;
    QDBusInterface *m_sppManager;
    console *m_serialConsole;
    QTimer *m_portCheckerTimer;
};

#endif // SERIALPORTMANAGER_H
