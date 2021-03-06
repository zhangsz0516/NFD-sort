// copyright (c) 2019 hors<horsicq@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "scanprogress.h"

ScanProgress::ScanProgress(QObject *parent) : QObject(parent)
{
    bIsStop=false;
    _pOptions=nullptr;
    currentStats=STATS();
    pElapsedTimer=nullptr;
    pSemaphore=nullptr;
//    connect(&futureWatcher, SIGNAL(finished()), this, SLOT(scan_finished()));
}

void ScanProgress::setData(QString sDirectoryName, ScanProgress::SCAN_OPTIONS *pOptions)
{
    this->_sDirectoryName=sDirectoryName;
    this->_pOptions=pOptions;
}

quint32 ScanProgress::getFileCount(quint32 nCRC)
{
    QMutexLocker locker(&mutex);

    quint32 nResult=0;

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec(QString("SELECT FILECOUNT FROM records where FILECRC='%1'").arg(nCRC));

    if(query.next())
    {
        nResult=query.value("FILECOUNT").toString().trimmed().toUInt();
    }

    if(query.lastError().text().trimmed()!="")
    {
        qDebug(query.lastQuery().toLatin1().data());
        qDebug(query.lastError().text().toLatin1().data());
    }

    return nResult;
}

void ScanProgress::setFileCount(quint32 nCRC, quint32 nCount)
{
    QMutexLocker locker(&mutex);

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec(QString("INSERT OR REPLACE INTO records(FILECRC,FILECOUNT) VALUES('%1','%2')").arg(nCRC).arg(nCount));

    if(query.lastError().text().trimmed()!="")
    {
        qDebug(query.lastQuery().toLatin1().data());
        qDebug(query.lastError().text().toLatin1().data());
    }
}

void ScanProgress::setFileStat(QString sFileName, QString sTimeCount, QString sDate)
{
    QMutexLocker locker(&mutex);

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec(QString("INSERT OR REPLACE INTO files(FILENAME,TIMECOUNT,DATETIME) VALUES('%1','%2','%3')")
               .arg(sFileName)
               .arg(sTimeCount)
               .arg(sDate));

    // QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")

    if(query.lastError().text().trimmed()!="")
    {
        qDebug(query.lastQuery().toLatin1().data());
        qDebug(query.lastError().text().toLatin1().data());
    }
}

void ScanProgress::createTables()
{
    QMutexLocker locker(&mutex);

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec("DROP TABLE if exists records");
    query.exec("DROP TABLE if exists files");
    query.exec("CREATE TABLE if not exists records(FILECRC text,FILECOUNT text,PRIMARY KEY(FILECRC))");
    query.exec("CREATE TABLE if not exists files(FILENAME text,TIMECOUNT text,DATETIME text,PRIMARY KEY(FILENAME))");
}

QString ScanProgress::getCurrentFileName()
{
    QMutexLocker locker(&mutex);

    QString sResult;

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec(QString("SELECT FILENAME FROM files where TIMECOUNT='' AND DATETIME='' LIMIT 1"));

    if(query.next())
    {
        sResult=query.value("FILENAME").toString().trimmed();
    }

    if(query.lastError().text().trimmed()!="")
    {
        qDebug(query.lastQuery().toLatin1().data());
        qDebug(query.lastError().text().toLatin1().data());
    }

    return sResult;
}

QString ScanProgress::getCurrentFileNameAndLock()
{
    QMutexLocker locker(&mutex);

    QString sResult;

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec(QString("SELECT FILENAME FROM files where TIMECOUNT='' AND DATETIME='' LIMIT 1"));

    if(query.next())
    {
        sResult=query.value("FILENAME").toString().trimmed();
    }

    if(query.lastError().text().trimmed()!="")
    {
        qDebug(query.lastQuery().toLatin1().data());
        qDebug(query.lastError().text().toLatin1().data());
    }

    query.exec(QString("INSERT OR REPLACE INTO files(FILENAME,TIMECOUNT,DATETIME) VALUES('%1','%2','%3')")
               .arg(sResult)
               .arg(0)
               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));

    if(query.lastError().text().trimmed()!="")
    {
        qDebug(query.lastQuery().toLatin1().data());
        qDebug(query.lastError().text().toLatin1().data());
    }

    return sResult;
}

qint64 ScanProgress::getNumberOfFile()
{
    QMutexLocker locker(&mutex);

    qint64 nResult=0;

    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec(QString("SELECT COUNT(FILENAME) AS VALUE FROM files where TIMECOUNT='' AND DATETIME=''"));

    if(query.next())
    {
        nResult=query.value("VALUE").toULongLong();
    }

    return nResult;
}

void ScanProgress::findFiles(QString sDirectoryName)
{
    if(!bIsStop)
    {
        QFileInfo fi(sDirectoryName);

        if(fi.isFile())
        {
            currentStats.nTotal++;
            setFileStat(fi.absoluteFilePath(),"","");
        }
        else if(fi.isDir()&&(_pOptions->bSubdirectories))
        {
            QDir dir(sDirectoryName);

            QFileInfoList eil=dir.entryInfoList();
            
            int nCount=eil.count();

            for(int i=0; (i<nCount)&&(!bIsStop); i++)
            {
                QString sFN=eil.at(i).fileName();

                if((sFN!=".")&&(sFN!=".."))
                {
                    findFiles(eil.at(i).absoluteFilePath());
                }
            }
        }
    }
}

void ScanProgress::startTransaction()
{
    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec("BEGIN TRANSACTION");
}

void ScanProgress::endTransaction()
{
    QSqlQuery query(_pOptions->dbSQLLite);

    query.exec("COMMIT");
}

void ScanProgress::_processFile(QString sFileName)
{
    pSemaphore->acquire();

    currentStats.nCurrent++;
    currentStats.sStatus=sFileName;

    if(currentStats.sStatus!="")
    {
        QString sTempFile;

        if(_pOptions->bDebug)
        {
            sTempFile=_pOptions->sResultDirectory;

            XBinary::createDirectory(sFileName);

            sTempFile+=QDir::separator()+XBinary::getBaseFileName(currentStats.sStatus);

            XBinary::copyFile(currentStats.sStatus,sTempFile);
        }

        SpecAbstract::SCAN_OPTIONS options={};

        options.bDeepScan=_pOptions->bDeepScan;
        options.bRecursiveScan=_pOptions->bRecursive;
        options.bSubdirectories=_pOptions->bSubdirectories;
        options.bIsTest=_pOptions->bIsTest;

        SpecAbstract::SCAN_RESULT scanResult=StaticScan::processFile(currentStats.sStatus,&options);

        int nCount=scanResult.listRecords.count();

        for(int i=0;i<nCount;i++)
        {
            SpecAbstract::SCAN_STRUCT ss=scanResult.listRecords.at(i);

            if(_pOptions->stFileTypes.contains(ss.id.filetype)&&_pOptions->stTypes.contains(ss.type))
            {
                QString sResult=SpecAbstract::recordNameIdToString(ss.name);

                if(ss.sVersion!="")
                {
                    sResult+=QString("(%1)").arg(ss.sVersion);
                }

                if(ss.sInfo!="")
                {
                    sResult+=QString("[%1]").arg(ss.sInfo);
                }

                sResult=XBinary::convertFileNameSymbols(sResult);

                quint32 nCRC=XBinary::getStringCustomCRC32(sResult);

                bool bCopy=true;

                int nCurrentCount=getFileCount(nCRC);

                if(_pOptions->nCopyCount)
                {
                    if(nCurrentCount>=_pOptions->nCopyCount)
                    {
                        bCopy=false;
                    }
                }

                if(bCopy)
                {
                    QString sFileName=_pOptions->sResultDirectory;

                    XBinary::createDirectory(sFileName);
                    sFileName+=QDir::separator()+SpecAbstract::recordFiletypeIdToString(ss.id.filetype);
                    XBinary::createDirectory(sFileName);
                    sFileName+=QDir::separator()+SpecAbstract::recordTypeIdToString(ss.type);
                    XBinary::createDirectory(sFileName);
                    sFileName+=QDir::separator()+sResult;
                    XBinary::createDirectory(sFileName);
                    sFileName+=QDir::separator()+XBinary::getBaseFileName(scanResult.sFileName);

                    if(XBinary::copyFile(scanResult.sFileName,sFileName))
                    {
                        setFileCount(nCRC,nCurrentCount+1);
                    }
                }
            }
        }

        setFileStat(scanResult.sFileName,QString::number(scanResult.nScanTime),QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

        if(_pOptions->bDebug)
        {
            XBinary::removeFile(sTempFile);
        }
    }

    pSemaphore->release();
}

void ScanProgress::scan_finished()
{
    qDebug("void ScanProgress::scan_finished()");
}

void ScanProgress::process()
{
    pSemaphore=new QSemaphore(N_MAXNUMBEROFTHREADS);
    pElapsedTimer=new QElapsedTimer;
    pElapsedTimer->start();

    if(!(_pOptions->bContinue))
    {
        createTables();
    }
    currentStats.nTotal=0;
    currentStats.nCurrent=0;

    bIsStop=false;

    currentStats.sStatus=tr("Directory scan");

    if(!(_pOptions->bContinue))
    {
        startTransaction();

        findFiles(_sDirectoryName);

        endTransaction();
    }

    currentStats.nTotal=getNumberOfFile();

    for(int i=0; (i<currentStats.nTotal)&&(!bIsStop); i++)
    {
        QString sFileName=getCurrentFileNameAndLock();

        if(sFileName=="")
        {
            break;
        }

        QFuture<void> future=QtConcurrent::run(this,&ScanProgress::_processFile,sFileName);

        QThread::msleep(100);

        while(true)
        {
            int nAvailable=pSemaphore->available();
            currentStats.nNumberOfThreads=N_MAXNUMBEROFTHREADS-nAvailable;
            if(nAvailable)
            {
                break;
            }

            QThread::msleep(500);
        }
    }

    while(true)
    {
        int nAvailable=pSemaphore->available();
        currentStats.nNumberOfThreads=N_MAXNUMBEROFTHREADS-nAvailable;

        if(nAvailable==N_MAXNUMBEROFTHREADS)
        {
            break;
        }

        QThread::msleep(1000);
    }

    delete pSemaphore;

    emit completed(pElapsedTimer->elapsed());
    delete pElapsedTimer;
    pElapsedTimer=nullptr;

    bIsStop=false;
}

void ScanProgress::stop()
{
    bIsStop=true;
}

ScanProgress::STATS ScanProgress::getCurrentStats()
{
    if(pElapsedTimer)
    {
        currentStats.nElapsed=pElapsedTimer->elapsed();
    }

    return currentStats;
}

bool ScanProgress::createDatabase(QSqlDatabase *pDb, QString sDatabaseName)
{
    bool bResult=false;

    *pDb=QSqlDatabase::addDatabase("QSQLITE", "sqllite");
    pDb->setDatabaseName(sDatabaseName);

    if(pDb->open())
    {
        QSqlQuery query(*pDb);

        query.exec("PRAGMA journal_mode = WAL");
        query.exec("PRAGMA synchronous = NORMAL");

        bResult=true;
    }

    return bResult;
}
