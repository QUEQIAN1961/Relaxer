#pragma once
#include "model.h"
namespace relaxer {
// Listening statistics require a unique match. Duplicate-download detection only
// needs one existing copy. Neither operation merges files or library rows.
qint64 matchOnlineTrack(const Track &online,const QVector<Track> &local,bool requireUnique=true);
}
