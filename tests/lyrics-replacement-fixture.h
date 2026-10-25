#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>

namespace replacement_fixture {
inline QByteArray mono(){return "[ti:New metadata]\n[ar:Synthetic]\n[00:01.00]Hello world\n[00:02.00]Walk home\n";}
inline QByteArray bilingual(){return QStringLiteral("[re:relaxer-bilingual]\n[ti:Old metadata]\n[ar:Synthetic]\n[by:Human or AI]\n[00:01.00]Hello world\n[00:01.00]世界你好\n[00:02.00]Walk home\n[00:02.00]走回家\n").toUtf8();}
inline QByteArray merged(){return QStringLiteral("[re:relaxer-bilingual]\n[by:Human or AI]\n[ti:New metadata]\n[ar:Synthetic]\n[00:01.00]Hello world\n[00:01.00]世界你好\n[00:02.00]Walk home\n[00:02.00]走回家\n").toUtf8();}
struct Case { const char *name; QByteArray old, candidate, expected; bool confirmation, preserves; };
inline QVector<Case> cases(){
    auto original=mono();original.replace("Hello world","Different words");
    auto timing=mono();timing.replace("00:01.00","00:01.50");
    auto legacy=bilingual();legacy.replace("[re:relaxer-bilingual]\n","");
    auto legacyMerged=merged();legacyMerged.replace("[re:relaxer-bilingual]\n","");
    return {{"new",{},mono(),mono(),false,false},
        {"saved-ai",bilingual(),mono(),merged(),false,true},
        {"saved-human",legacy,mono(),legacyMerged,false,true},
        {"different-original",bilingual(),original,original,true,false},
        {"different-time",bilingual(),timing,timing,true,false},
        {"instrumental",bilingual(),QStringLiteral("纯音乐，请欣赏").toUtf8(),QStringLiteral("纯音乐，请欣赏").toUtf8(),true,false}};
}
}
