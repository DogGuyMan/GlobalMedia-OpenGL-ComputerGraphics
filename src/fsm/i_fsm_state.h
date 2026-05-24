#ifndef __SJH_FSM_I_FSM_STATE_H__
#define __SJH_FSM_I_FSM_STATE_H__

#include <cstdint>
namespace SJH::FSM
{
    /// @brief State Entity 베이스. owner 컨텍스트를 hook 인자로 받는다.
    /// @tparam TOwner 보통 Actor 파생 클래스 (e.g. PlayerActor).
    template<typename TOwner>
    class IFsmState
    {
    public:
    virtual ~IFsmState() = default;
    	virtual uint64_t GetStateFlag() = 0;
        virtual void OnEnter(TOwner& owner)              = 0;
        virtual void OnUpdate(TOwner& owner, float dt)   = 0;
        virtual void OnExit(TOwner& owner)               = 0;
    };

}  // namespace SJH::FSM

#endif // __SJH_FSM_I_FSM_STATE_H__
