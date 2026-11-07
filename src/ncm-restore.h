#pragma once
#include <QString>
namespace relaxer {
struct NcmRestoreResult {QString path,error;};
NcmRestoreResult restoreNcm(const QString &input,const QString &outputBase);
}
