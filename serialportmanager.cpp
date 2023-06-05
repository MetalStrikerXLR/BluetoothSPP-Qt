#include "serialportmanager.h"
#define SPP_PREFIX 0x1101

SerialPortManager::SerialPortManager(QObject *parent) : QObject{parent}
{
    // Initialize Console
    m_serialConsole = new console();

    // Initialize Serial Port Scanner Timer
    m_portCheckerTimer = new QTimer();
    m_portCheckerTimer->setSingleShot(true);
    m_portCheckerTimer->setInterval(1000);
    QObject::connect(m_portCheckerTimer, &QTimer::timeout, this, &SerialPortManager::checkPortAvailable);

    // Configure Serial Port
    m_serialPort = new QSerialPort();
    m_serialPort->setPortName("rfcomm0");
    m_serialPort->setBaudRate(QSerialPort::Baud9600);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    QObject::connect(this, &SerialPortManager::rfcommPortAvailable, this, &SerialPortManager::connectSerialPort);

    // Register Metatype and define System Bus
    qDBusRegisterMetaType <InterfacesMap> ();
    qDBusRegisterMetaType <ObjectsMap> ();
    auto systemBus = QDBusConnection::systemBus();

    // Create Interface for Bluez ProfileManager1
    // Interface Used for Profile Registeration
    m_sppManager = new QDBusInterface("org.bluez",
                                      "/org/bluez",
                                      "org.bluez.ProfileManager1",
                                      systemBus);

    // Create Interface for Bluetooth Manager
    // Interface Used to Monitor Bluetooth Registered/Connected Devices and States
    m_blueManager = new QDBusInterface("org.bluez",
                                       "/",
                                       "org.freedesktop.DBus.ObjectManager",
                                       systemBus);

    // Check if a New Bluetooth device is connected to Adapter
    // Call 'onInterfacesAdded' slot if so
    if (!systemBus.connect("org.bluez",
                           "/",
                           "org.freedesktop.DBus.ObjectManager",
                           "InterfacesAdded",
                           this,
                           SLOT(onInterfacesAdded(const QDBusObjectPath &, const InterfacesMap)))) {
        qWarning() << "Failed to connect InterfacesAdded signal";
    }

    if (!systemBus.connect("org.bluez",
                           "/",
                           "org.freedesktop.DBus.ObjectManager",
                           "InterfacesRemoved",
                           this,
                           SLOT(onInterfacesRemoved(const QDBusObjectPath &, const InterfacesMap)))) {
        qWarning() << "Failed to connect InterfacesRemoved signal";
    }

    registerProfileSPP(m_sppManager);
    fetchObjects(m_blueManager);
}

SerialPortManager::~SerialPortManager()
{
    delete m_serialPort;
    delete m_blueManager;
    delete m_sppManager;
    delete m_serialConsole;
    delete m_portCheckerTimer;
}

void SerialPortManager::registerProfileSPP(QDBusInterface *manager)
{
    // Define Profile UUID and Service Record
    QString SPP_UUID = "00001101-0000-1000-8000-00805f9b34fb";
    QString service_record = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
                                            "<record>\n"
                                            "  <attribute id=\"0x0001\">\n"
                                            "    <sequence>\n"
                                            "      <uuid value=\"0x1101\"/>\n"
                                            "    </sequence>\n"
                                            "  </attribute>\n"
                                            "  <attribute id=\"0x0004\">\n"
                                            "    <sequence>\n"
                                            "      <sequence>\n"
                                            "        <uuid value=\"0x0100\"/>\n"
                                            "      </sequence>\n"
                                            "      <sequence>\n"
                                            "        <uuid value=\"0x0003\"/>\n"
                                            "        <uint8 value=\"1\" name=\"channel\"/>\n"
                                            "      </sequence>\n"
                                            "    </sequence>\n"
                                            "  </attribute>\n"
                                            "  <attribute id=\"0x0100\">\n"
                                            "    <text value=\"Serial Port\" name=\"name\"/>\n"
                                            "  </attribute>\n"
                                            "</record>\n");

    // Define Arguments for Profile Registeration
    QVariantMap args;
    args["Name"] = "MConn SPP Profile";
    args["Service"] = "00001101-0000-1000-8000-00805F9B34FB";
    args["Role"] = "server";
    args["AutoConnect"] = "True";
    args["Requirements"] = QStringList();
    args["Channel"] = 22;
    args["PSM"] = "3";
    args["ServiceRecord"] = service_record;

    // Call 'RegisterProfile' Method in ProfileManager1 and Check Registeration Success
    QDBusReply<void> reply = manager->call("RegisterProfile", QVariant::fromValue(QDBusObjectPath("/bluez")), SPP_UUID, args);
    if (reply.isValid()) {
        qDebug() << "SPP Profile Successfully Registered!";
        m_serialConsole->execute("killall rfcomm");
        m_serialConsole->execute("rfcomm watch hci0 &");
        qDebug() << "rfcomm watcher started.";
    }
    else {
        qDebug() << "Failed to Register SPP Profile: " << reply.error().message();
    }
}

void SerialPortManager::fetchObjects(QDBusInterface *manager)
{
    QDBusReply<QMap<QDBusObjectPath,QMap<QString,QVariantMap>>> reply;
    reply = manager->call("GetManagedObjects");
    if (!reply.isValid()) {
        qDebug() << "Failed to connect to bluez: " << reply.error().message();
        return;
    }

    auto objects = reply.value(); // contains the main bluetooth adapter and all device interfaces

    for (auto i = objects.begin(); i != objects.end(); ++i) {
        auto ifaces = i.value();

        for (auto j = ifaces.begin(); j != ifaces.end(); ++j) {
            if (j.key() == "org.bluez.Device1") {
                qDebug() << "Registering Already Paired Device: ";
                registerDevice(i.key());
            }
        }
    }
}

void SerialPortManager::registerDevice(QDBusObjectPath path)
{
    qDebug() << path.path();

    // Register Bluetooth Device
    // Call 'onPropertiesChanged' Slot if any Property of the Device Changes
    auto systemBus = QDBusConnection::systemBus();
    if (!systemBus.connect("org.bluez",
                           path.path(),
                           "org.freedesktop.DBus.Properties",
                           "PropertiesChanged",
                           this,
                           SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
        qWarning() << "Failed to connect propertyChanged signal";
    }
}

void SerialPortManager::onInterfacesAdded(const QDBusObjectPath &path, const InterfacesMap interfaces)
{
    for (auto i = interfaces.begin(); i != interfaces.end(); ++i) {
        if (i.key() == "org.bluez.Device1") {
            qDebug() << "Registering New Device: ";
            registerDevice(path);
        }
    }
}

void SerialPortManager::onInterfacesRemoved(const QDBusObjectPath &path, const InterfacesMap interfaces)
{
    for (auto i = interfaces.begin(); i != interfaces.end(); ++i) {
        if (i.key() == "org.bluez.Device1") {
            qDebug() << "Device Unregistered: " << path.path();

            auto systemBus = QDBusConnection::systemBus();
            if (!systemBus.disconnect("org.bluez",
                                      path.path(),
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged",
                                      this,
                                      SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
                qWarning() << "Failed to disconnect PropertiesChanged signal";
            }
            break;
        }
    }
}

void SerialPortManager::onPropertiesChanged(QString interface, QVariantMap changed_properties, QStringList invalidated_properties)
{
    if (changed_properties["Connected"] != QVariant(QVariant::Invalid)) {

        if (changed_properties["Connected"].toBool() == true) {
            qDebug() << "Device Connected";
            m_portCheckerTimer->start();
        }

        if (changed_properties["Connected"].toBool() == false) {
            qDebug() << "Device Disconnected";
            disconnect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortManager::readMessage);
            m_serialPort->close();
            m_serialConsole->execute("killall rfcomm");
            m_serialConsole->execute("rfcomm watch hci0 &");
            qDebug() << "Restarted rfcomm watcher Service";
        }
    }
}

void SerialPortManager::checkPortAvailable()
{
    // Check if rfcomm0 is available or not
    QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : availablePorts) {
        if(portInfo.portName() == "rfcomm0") {
            emit rfcommPortAvailable();
            return;
        }
    }
}

void SerialPortManager::connectSerialPort()
{
    qDebug() << "Device Supports Serial Capabilities!";

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "Serial Connection Established...";
        connect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortManager::readMessage);
        writeMessage("Greetings from MConn!");
    }
    else {
        qDebug() << "Serial Connection could not be Established...";
    }
}

void SerialPortManager::writeMessage(QString msg)
{
    m_serialPort->write(msg.toUtf8());
}

void SerialPortManager::readMessage()
{
    QString data = m_serialPort->readAll();
    writeMessage("ACK: " + data);

    qDebug() << "Received data:" << data;

    emit messageReceived(data);
}


