#include "console.h"

console::console(QObject *parent) : QObject(parent)
{

}

// Execute command and read output
QString console::execute(QString command)
{
    // Create process
    QProcess process;

    // Execute command
    process.start("bash", QStringList() << "-c" << command);
    process.waitForFinished();

    // Filter payload
    QString payload = process.readAllStandardOutput();

    if(payload == "")
        payload = process.readAllStandardError();

    // Close the process
    process.close();

    // Return payload
    return payload;
}

bool console::fileExists(QString fileName)
{
    if(fileName.isEmpty())
        return false;

    if(QFileInfo::exists(fileName))
    {
        return true;
    }
    else
    {
        return false;
    }
}

QString console::fileRead(QString fileName)
{
    QFile file(fileName);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return "";
    }

    QTextStream instream(&file);
    QString line = instream.readLine();
    file.close();

    return line;
}

bool console::fileRemove(QString fileName)
{
    if(fileExists(fileName))
    {
        try {
            QFile file (fileName);
            file.remove();
        }  catch (...) {
            return false;
        }
        return true;
    }
    return false;
}

bool console::fileTouch(QString fileName)
{
    if(execute("touch \"" + fileName + "\"").isEmpty())
        return true;

    return false;
}
