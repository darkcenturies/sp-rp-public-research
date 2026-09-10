#pragma once
#include <cstdio>
#include <string>
#include <cstring>

namespace crashfixsupport {
inline bool IsName(const char* name) {
    return !_stricmp(name,"valkyrie-crashfix.asi") || !_stricmp(name,"Valkyrie Crashfix.asi");
}
// The released ASI embeds this startup marker. Read it from the loaded image,
// not an unrelated backup or a different file currently present on disk.
inline std::string Version(const std::string& bytes) {
    const std::string prefix="Valkyrie Crashfix ";size_t start=bytes.find(prefix);
    while(start!=std::string::npos) {
        start+=prefix.size();size_t end=start;
        while(end<bytes.size() && ((bytes[end]>='0' && bytes[end]<='9') || bytes[end]=='.'))++end;
        if(end>start && bytes.compare(end,9," starting")==0)return bytes.substr(start,end-start);
        start=bytes.find(prefix,start);
    }
    return {};
}
inline bool Older(const std::string& version) {
    unsigned major=0,minor=0,patch=0;
    if(sscanf_s(version.c_str(),"%u.%u.%u",&major,&minor,&patch)<2)return false;
    return major<2 || (major==2 && (minor<2 || (minor==2 && patch<2)));
}
inline std::string Advice(bool present,const std::string& version,bool spanish) {
    const char* url="https://sp-rp.com/#valkyrie-crashfix";
    std::string result=spanish?"\n\nVALKYRIE CRASHFIX: ":"\n\nVALKYRIE CRASHFIX: ";
    if(!present)result+=spanish?"No esta cargado. Instale o active Crashfix 2.2.2 o posterior desde ":"Not loaded. Install or enable Crashfix 2.2.2 or later from ";
    else if(Older(version))result+=(spanish?"Version antigua cargada: ":"Older version loaded: ")+version+(spanish?". Actualice desde ":". Update from ");
    else if(version.empty())result+=spanish?"Cargado; version desconocida. Compare su copia con ":"Loaded; version unrecognized. Compare your copy with ";
    else return result+(spanish?"Version cargada: ":"Loaded version: ")+version+(spanish?". Consulte Valkyrie Crashfix.log para ver los parches instalados u omitidos. No cubre todos los fallos.":". Check Valkyrie Crashfix.log for installed or skipped guards. This does not cover every crash.");
    result+=url;
    result+=spanish?". Mantenga una sola copia activa. Esta recomendacion no identifica la causa del fallo.":". Keep one active copy. This recommendation does not identify the cause of this crash.";
    return result;
}
}
