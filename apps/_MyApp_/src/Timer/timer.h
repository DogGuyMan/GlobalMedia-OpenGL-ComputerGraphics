#ifndef __TOPDOWNSHOOTER_TIMER_H__
#define __TOPDOWNSHOOTER_TIMER_H__

namespace TopdownShooter
{
	class Timer
	{
	};
}; // namespace TopdownShooter

/*

using UnityEngine;
using UnityEngine.Events;

namespace Sophia.DataSystem.Modifiers
{
    using System;
    using System.Threading;
    using Sophia.Composite.NewTimer;
    using Sophia.Entitys;
    using Sophia.State;

    public abstract class Affector : IStateMachine<AffectorState, Entitys.Entity>, ITimerAccessible<Entity>
    {
#region Members

        public E_AFFECT_TYPE AffectType {get; protected set;}
        public string Name {get; protected set;}
        public string Description {get; protected set;}
        public Sprite Icon {get; protected set;}
        protected TimerComposite Timer;

        public Affector(in SerialAffectorData affectData)
        {
            this.Init(in affectData);
            OnClear += ResetState;
        }

        protected abstract void Init(in SerialAffectorData affectData);
#endregion

#region Event

        public event UnityAction<Affector> OnClear;
        public void ClearAffect(Affector affector) => OnClear?.Invoke(affector);

#endregion

#region State Machine

        protected AffectorState CurrentState;
        public AffectorState GetCurrentState() => CurrentState;
        public void ChangeState(AffectorState newState) {
            if(newState == null) return;
            if(GetIstransferableState(newState)) CurrentState = newState;
        }
        public void ExecuteState(Entity entity) => CurrentState.Affect(this, entity);
        public bool GetIstransferableState(AffectorState transState) {
            return (CurrentState.GetTransitionBit() & transState.GetCurrentBit()) == transState.GetCurrentBit();
        }
        public void ResetState(Affector affector) => ResetState();
        public void ResetState() {
            this.Timer.ResetTimer();
            this.CurrentState = AffectorReadyState.Instance;
            this.OnClear += ResetState;
        }

#endregion

#region Timer

        public TimerComposite GetTimerComposite() => Timer;
        public abstract void Enter(Entity entity);
        public abstract void Run(Entity entity);
        public virtual void Exit(Entity entity) { OnClear?.Invoke(this); }

#endregion

    }

    public interface AffectorState : ITransitionAccessible{
        public void Affect(Affector affector, Entity entity);
    }

    public class AffectorReadyState : AffectorState
    {
        private static AffectorReadyState _instance = new AffectorReadyState();
        public static AffectorReadyState Instance => _instance;
        public void Affect(Affector affector, Entity entity)
        {
            affector.ChangeState(AffectorStartState.Instance);
        }

        public int GetCurrentBit() => (int)TimerStateBit.Ready;
        public int GetTransitionBit() => (int)TimerStateBit.Start;

    }

    public class AffectorStartState : AffectorState
    {
        private static AffectorStartState _instance = new AffectorStartState();
        public static AffectorStartState Instance => _instance;

        public void Affect(Affector affector, Entity entity)
        {
            affector.Enter(entity);
            affector.ChangeState(AffectorRunState.Instance);
        }

        public int GetCurrentBit() => (int)TimerStateBit.Start;
        public int GetTransitionBit() => (int)TimerStateBit.Run;
    }

    public class AffectorRunState : AffectorState
    {
        private static AffectorRunState _instance = new AffectorRunState();
        public static AffectorRunState Instance => _instance;

        public void Affect(Affector affector, Entity entity)
        {
            if(affector.GetTimerComposite().IsBlocked)      { affector.ChangeState(AffectorPauseState.Instance);        return;}
            if(affector.GetTimerComposite().GetIsTimesUp()) { affector.ChangeState(AffectorTerminateState.Instance);    return;}
            if(affector.GetTimerComposite().GetIsActivateInterval()) {
                affector.Run(entity);
            }
        }

        public int GetCurrentBit() => (int)TimerStateBit.Run;
        public int GetTransitionBit() => (int)TimerStateBit.Terminate + (int)TimerStateBit.Pause;
    }

    public class AffectorPauseState : AffectorState
    {
        private static AffectorPauseState _instance = new AffectorPauseState();
        public static AffectorPauseState Instance => _instance;
        public void Affect(Affector affector, Entity entity)
        {
            if(!affector.GetTimerComposite().IsBlocked) {affector.ChangeState(AffectorRunState.Instance); return;}
        }

        public int GetCurrentBit() => (int)TimerStateBit.Pause;
        public int GetTransitionBit() => (int)TimerStateBit.Terminate + (int)TimerStateBit.Run;
    }

    public class AffectorTerminateState : AffectorState
    {
        private static AffectorTerminateState _instance = new AffectorTerminateState();
        public static AffectorTerminateState Instance => _instance;
        public void Affect(Affector affector, Entity entity)
        {
            affector.Exit(entity);
        }

        public int GetCurrentBit() => (int)TimerStateBit.Terminate;
        public int GetTransitionBit() => 0;
    }

}
    using System;

    namespace Sophia.Composite
    {
        namespace NewTimer
        {
            public interface ITimer<Receiver> {
                public void Enter(Receiver receiver);
                public void Run(Receiver receiver);
                public void Exit(Receiver receiver);
            }

            public interface ITimerAccessible<Receiver> : ITimer<Receiver>{
                public TimerComposite GetTimerComposite();
            }

            public class TimerComposite
            {

    #region Member

                public readonly float BaseTime;
                public float AccelerationAmount = 1f; // Ratio;

                private float mPassedTime;
                public float PassedTime
                {
                    get { return mPassedTime; }
                    internal set
                    {
                        if (value <= 0) { mPassedTime = 0; return; }
                        if (value >= BaseTime) { mPassedTime = BaseTime; return; }
                        mPassedTime = value;
                    }
                }
                public bool IsBlocked { get; internal set; }

                public TimerComposite(float baseTime)
                {
                    this.BaseTime = baseTime;
                    PassedTime = 0;
                    IsBlocked = false;
                }

                public TimerComposite SetAcceleratrion(float amount)
                {
                    if (amount <= 0) { amount = 0; }
                    AccelerationAmount = amount;
                    return this;
                }
    #endregion

    #region Interval

                public IntervalTimerComposite intervalTimer;
                public TimerComposite SetInterval(float interval)
                {
                    intervalTimer = new IntervalTimerComposite(interval);
                    return this;
                }
                public bool GetIsActivateInterval() => intervalTimer == null ? false : intervalTimer.GetIsActivateInterval(PassedTime);

    #endregion

    #region Rewind

                public RewaindTimerComposite rewaindTimer;
                public TimerComposite SetRewaind(Func<bool> condition)
                {
                    rewaindTimer = new RewaindTimerComposite(condition);
                    return this;
                }

                public bool GetIsRewainable() => rewaindTimer == null ? false : rewaindTimer.GetIsRewainable();

    #endregion

                public void FrameTick(float passedTick) {PassedTime += passedTick;}
                public float GetProgressAmount() { return PassedTime / BaseTime; }
                public bool GetIsTimesUp() {return PassedTime >= BaseTime;}
                public void Puase() => IsBlocked = true;
                public void Continue() => IsBlocked = false;

                public void ResetTimer() {
                    PassedTime = 0;
                    IsBlocked = false;
                    intervalTimer?.ResetNextInterval();
                }
            }

            public class IntervalTimerComposite
            {
                public float IntervalTime { get; private set; }
                internal float NextInterval;

                public IntervalTimerComposite(float intervel)
                {
                    IntervalTime = intervel;
                    NextInterval = IntervalTime;
                }
                public bool GetIsActivateInterval(float PassedTime)
                {
                    if (PassedTime >= 0f && PassedTime >= NextInterval)
                    {
                        NextInterval += IntervalTime;
                        return true;
                    }
                    return false;
                }
                public void ResetNextInterval() {
                    NextInterval = IntervalTime;
                }
            }

            public class RewaindTimerComposite
            {
                public bool IsLoop { get; internal set; }
                public Func<bool> WhenRewindable;
                public RewaindTimerComposite(Func<bool> condition)
                {
                    WhenRewindable = condition;
                }
                public bool GetIsRewainable() =>  WhenRewindable.Invoke();
                public void ClearRewindCondition() => WhenRewindable = null;
            }
        }
    }
    */
#endif //__TOPDOWNSHOOTER_TIMER_H__
