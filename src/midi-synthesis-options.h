#pragma once
#include <QJsonObject>
namespace relaxer {
// The persisted settings and the playback startup snapshot share one policy.
inline QJsonObject midiSynthesisDefaults(){return {{"reverb",true},{"reverbLevel",70},{"chorus",true},{"chorusLevel",60},{"interpolation",4},{"polyphony",256}};}
inline QJsonObject normalizedMidiSynthesis(const QJsonObject &input){
    auto result=midiSynthesisDefaults();
    for(const auto &key:{"reverb","chorus"})if(input.value(key).isBool())result[key]=input.value(key);
    for(const auto &key:{"reverbLevel","chorusLevel","interpolation","polyphony"}){
        const auto v=input.value(key);if(!v.isDouble()||v.toDouble()!=v.toInt())continue;
        const int n=v.toInt();
        if((QString(key)=="interpolation"&&(n==4||n==7))||
           (QString(key)=="polyphony"&&n>=128&&n<=2048)||
           ((QString(key)=="reverbLevel"||QString(key)=="chorusLevel")&&n>=0&&n<=100))result[key]=n;
    }
    return result;
}
}
