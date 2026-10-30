#pragma once
#ifdef Q_OS_WIN
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <iostream>
#include <windows.h>

namespace foreground_test {
inline QJsonObject snapshot(HWND owned,const QString &phase,DWORD subjectPID=GetCurrentProcessId()) {
    const auto window=GetForegroundWindow();DWORD pid=0;
    const auto thread=window?GetWindowThreadProcessId(window,&pid):0;
    wchar_t className[256]{};if(window)GetClassNameW(window,className,256);
    QString processName;DWORD processError=0;
    if(pid){
        const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(process){wchar_t path[32768]{};DWORD size=32768;
            if(QueryFullProcessImageNameW(process,0,path,&size))processName=QFileInfo(QString::fromWCharArray(path,int(size))).fileName();
            else processError=GetLastError();CloseHandle(process);
        }else processError=GetLastError();
    }
    return {{"phase",phase},{"utcMs",double(QDateTime::currentMSecsSinceEpoch())},
        {"state",!window?"none":window==owned?"owned":"unowned"},
        {"foreground",QString::number(quintptr(window),16)},{"owned",QString::number(quintptr(owned),16)},
        {"ownerPID",double(pid)},{"ownerThread",double(thread)},{"processName",processName},
        {"processQueryError",double(processError)},{"windowClass",QString::fromWCharArray(className)},
        {"testPID",double(subjectPID)},{"observerPID",double(GetCurrentProcessId())},
        {"observerThread",double(GetCurrentThreadId())},{"externalProcess",window&&pid!=subjectPID}};
}
// This mode runs in a separate process, before QApplication or any test window.
// It observes only; no hooks, window creation or input APIs are used.
inline int witnessMain(){
    std::string line;
    while(std::getline(std::cin,line)){
        const auto request=QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
        bool valid=false;const auto value=request["owned"].toString().toULongLong(&valid,16);
        if(!valid)return 2;
        const auto result=snapshot(reinterpret_cast<HWND>(quintptr(value)),request["phase"].toString(),DWORD(request["subjectPID"].toDouble()));
        std::cout<<QJsonDocument(result).toJson(QJsonDocument::Compact).constData()<<std::endl;
    }
    return 0;
}
class Witness {
    QProcess process;
public:
    bool start(){
        process.start(QCoreApplication::applicationFilePath(),{"--rc5-foreground-witness"});
        return process.waitForStarted(3000);
    }
    QJsonObject observe(HWND owned,const QString &phase){
        const auto before=snapshot(owned,phase+"/local-before");
        const auto request=QJsonDocument(QJsonObject{{"phase",phase+"/independent-process"},
            {"owned",QString::number(quintptr(owned),16)},{"subjectPID",double(GetCurrentProcessId())}}).toJson(QJsonDocument::Compact)+'\n';
        if(process.write(request)!=request.size())return {{"valid",false},{"error","witness write failed"}};
        while(process.bytesToWrite())if(!process.waitForBytesWritten(1000))return {{"valid",false},{"error","witness write timeout"}};
        while(!process.canReadLine())if(!process.waitForReadyRead(1000))return {{"valid",false},{"error","witness response timeout"}};
        const auto external=QJsonDocument::fromJson(process.readLine()).object();
        const auto after=snapshot(owned,phase+"/local-after");
        const bool valid=external.contains("foreground")&&external["observerPID"].toDouble()!=double(GetCurrentProcessId());
        const bool consistent=valid&&before["foreground"]==external["foreground"]&&external["foreground"]==after["foreground"];
        return {{"phase",phase},{"valid",valid},{"consistent",consistent},
            {"localBefore",before},{"independentProcess",external},{"localAfter",after}};
    }
    ~Witness(){
        process.closeWriteChannel();
        if(process.state()!=QProcess::NotRunning&&!process.waitForFinished(2000)){process.kill();process.waitForFinished(1000);}
    }
};
}
#endif
