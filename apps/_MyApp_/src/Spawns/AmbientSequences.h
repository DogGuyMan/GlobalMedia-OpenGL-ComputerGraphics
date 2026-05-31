#ifndef __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__

#include "Spawns/SequenceContext.h"

namespace TopdownShooter::Spawns
{
    /// @brief BGM — sceneRoot 밑 "BgmActor" 에 FmodStudioPlayable(event:/BGM) loop 무한 부착.
    /// @details 단발(one-shot) 아님 — 지속 재생이라 fxRoot/AudioInstance 대신 sceneRoot 직접 부착 + SetIsLoop(true).
    void BuildBGM(const SequenceContext& ctx);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__
