#ifndef CONSOLE_H
#define CONSOLE_H

#include <QObject>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QFileInfo>

class console : public QObject
{
    Q_OBJECT
public:
    explicit console(QObject *parent = nullptr);

    QString execute(QString command);

    bool fileExists(QString fileName);

    QString fileRead(QString fileName);

    bool fileRemove(QString fileName);

    bool fileTouch(QString fileName);

signals:

};

#endif // CONSOLE_H
