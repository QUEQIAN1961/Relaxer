#include "library.h"
#include "lyrics-replacement-fixture.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace relaxer;
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QCommandLineParser parser;
    parser.addHelpOption();parser.addOption({"data","Create a synthetic D2-03 fixture in a new or empty directory","directory"});parser.process(app);
    const auto path=parser.value("data");
    if(path.isEmpty() || (QFileInfo::exists(path) && (!QFileInfo(path).isDir() || !QDir(path).entryList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty()))) {
        QTextStream(stderr)<<"An explicit new or empty --data directory is required; nothing was changed.\n";return 64;
    }
    Library library(QFileInfo(path).absoluteFilePath());if(!library.open()){QTextStream(stderr)<<library.lastError()<<'\n';return 2;}
    const QStringList titles={"D203 01 无歌词","D203 02 单语","D203 03 人工双语","D203 04 文件offset与显示延时","D203 05 已标记纯音乐"};
    QJsonArray entries;
    for(int i=0;i<titles.size();++i) {
        QByteArray wave;QDataStream stream(&wave,QIODevice::WriteOnly);stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF",4);stream<<quint32(48036);stream.writeRawData("WAVEfmt ",8);
        stream<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(8000)<<quint32(16000)<<quint16(2)<<quint16(16);
        stream.writeRawData("data",4);stream<<quint32(48000);wave.append(QByteArray(48000,'\0'));wave[wave.size()-2]=char(i+1);
        Track t;t.hash=QString::fromLatin1(QCryptographicHash::hash(wave,QCryptographicHash::Sha256).toHex());t.title=titles[i];t.artist="Synthetic D2-03";t.file="Music/"+t.hash+".wav";t.duration=3000;
        QFile media(library.absolute(t.file));if(!media.open(QIODevice::WriteOnly)||media.write(wave)!=wave.size())return 3;media.close();
        t.id=library.insert(t);if(!t.id)return 4;
        auto words=i==1?replacement_fixture::mono():replacement_fixture::bilingual();
        if(i==2)words.replace("[re:relaxer-bilingual]\n","");
        if(i==3)words=QByteArray("[offset:250]\n")+words;
        if(i==4)words=QStringLiteral("纯音乐，请欣赏").toUtf8();
        if(i && !library.saveLyricsPreservingText(t.id,words))return 5;
        if(i==3 && !library.setLyricOffset(t.id,700))return 6;
        entries.append(QJsonObject{{"id",QString::number(t.id)},{"title",t.title},{"audio",library.absolute(t.file)},
            {"lyrics",i?library.absolute(library.track(t.id).lyrics):QString()},
            {"fileOffset",i==3?250:0},{"displayDelay",library.lyricOffset(t.id)},{"instrumental",library.isInstrumental(t.id)}});
    }
    const auto bytes=QJsonDocument(QJsonObject{{"kind","synthetic-only"},{"dataRoot",library.root()},{"tracks",entries}}).toJson();
    QFile manifest(library.absolute("D203-FIXTURE.json"));if(!manifest.open(QIODevice::WriteOnly)||manifest.write(bytes)!=bytes.size())return 7;
    QTextStream(stdout)<<QString::fromUtf8(bytes);return 0;
}
