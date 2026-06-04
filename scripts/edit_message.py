import os
import sys
import subprocess

# 사용자가 제공한 해시 및 커밋 목록 (최신 -> 과거)
RAW_COMMITS = """
53bc25e feat(vfx): main 로드 루프에 .efk 텍스처 검증 배선
32db244 feat(vfx): EffekseerPlayable::OnPlay 핸들 lifecycle 진단 배선
0e8836c docs(timer): PollInterval 이연 발사 계약 명시 (코드리뷰 Minor #1)
79e8902 feat(diagnostics): Effekseer 진단 EffekseerDiagnostics 신설 (의존성 0)
3bc31c8 docs(handoff): M6 + PlayerBehavior 분해 설계 핸드오프 보존 (2026-06-01)
26cfc3a feat(_MyApp_): PlayerBuilder FRONT_MOVE 4-레이어 배선 + clipStorage 제거
53a8858 feat(_MyApp_): CreatePlayerActor .cpp화 + SpriteCfg + 4-레이어 합성 로직
1d8cfe6 feat(sprite): SpriteSequencePlayable 값-소유 ctor — clip lifetime footgun 제거
979b5fe feat(buffer): Framebuffer::Resize — color+RBO in-place 재할당 (RBO 모드 전용)
2ff1c6b refactor(_MyApp_/M6): BGM 인라인 → Spawns::BuildBGM 이관 (Task 5)
326f19a feat(_MyApp_/M6): 단발 빌더 — VfxInstance + CombatSequences(HitSpark/DeathFX/Pickup) + AmbientSequences(BGM)
3fb0c6f feat(_MyApp_/M6): fxRoot 컨테이너 + SweepFinishedChildren 배선 (Fog 리펙토링 위 재통합)
ce0e2c9 feat(_MyApp_/M6): Spawns lib 골격 — SequenceContext + AutoDespawnOnFinish + OneShotSweeper(FindChildIf)
b09d2b2 feat(scene): Actor DetachChild/FindChild/FindChildIf — 트리 탐색·소유권 이전 API
7d5dd2e refactor(scene+render+registry): SceneContext + ResourceRegistry::GetAllPrograms() 일괄
57e6580 refactor(common+apps): Const::MAX_*_LIGHTS=16 + migrate_demo 활성 (Phase 1.5 마무리)
ee9b6eb build(engine): SJH::engine 우산에 playable + sprite_sequence 합류 (14 → 16 모듈)
fa02c3b fix(sprite_sequence): sprite_sequence_playable.cpp strict-include 정리
59eefcb feat(sprite_sequence): SJH::sprite_sequence 모듈 신설 — SpriteFrameClip + SpriteSequencePlayable
2eb81d8 fix(playable): composite_playable strict-include + sign-conversion 정리
76e1b03 feat(playable): SequencePlayable + ParallelPlayable + fluent Builder
b383639 feat(playable): SJH::playable 모듈 신설 — IPlayable + PlayableBase
642040b fix(material): Program 참조 복구 + EagerBuild 도입 — Observer 제거 후속
7b95332 docs(render): DeviceContext / Uniforms / Program 책임 경계 명시 (Phase 1)
251277c chore(_MyApp_): 빌드 잡음 청소 — physics_movement.cpp empty 삭제 + duplicate library 경고 silencing
73ec685 refactor(render): DrawCommand 를 MeshRenderer 단일 의존으로 통합 — program/mesh/material/actor 직접 필드 제거
fc4ad0a refactor(scene/render): Camera 의 RenderTarget 의존 역전 — Framebuffer 직접 의존 제거
64330dd docs(progress): M3 완료 반영 + spec 결정 #18 진화 회고
9aaa787 feat(_MyApp_): wall + pickup 시각화 — simple.vs/fs (단색 평면) MeshRenderer 부착
538b317 refactor(_MyApp_): PhysicsBodyComponent → Components::Physics base + BoxBody/CircleBody 구체화
3ef1dab refactor(_MyApp_): Physics::Filter constexpr → enum class PhysicsLayer : uint64_t
1a874f0 refactor(_MyApp_): IContactable / IPhysicsContactListener 중복 인터페이스 통합 (M3 follow-up)
e9954d1 fix(test): SP5 이전 API 불일치 수정 — 테스트 145/147 통과
def527c feat(_MyApp_): M3 Box2D v2.4.1 물리 통합 (Client 한정) + Unity isTrigger 패턴
254e781 milestone(SP5): IRenderStage + Layer 시스템 정착 완료
e14dd81 feat(tweeny_demo): SceneCamera Overlay + IRenderStage 패턴 (SP5)
2eb4150 feat(_MyApp_): M2 P2 정착 + M4 도메인 선행 — Player WASD + Movement 추상화 + Entity Components
c4fd046 feat(sprite): SpriteAnimator + UniformAtlas Fluent Builder 분리
3bdf499 refactor(fsm): TTransit 제거 + GetTransitFlag 기반 그래프 정보 응집
6f5346c refactor(fsm): 파일/클래스 명 정리 + P1 StateMachineProcessor 폐기
16a6cdd feat(fsm): ObjectStateMachine + IFsmState 추가 (M2 P1.5)
c1012ea feat(_MyApp_): M2 Player WASD + TargetFollowableCameraController
78dea2a feat(fsm): SJH::fsm 코어 모듈 신설 + StateMachineProcessor template
b590bb1 fix(_MyApp_): M1.5 빌보드 정면 + Mesh::CreatePlane + Scene::Camera 의존
173f3c4 feat(_MyApp_): M1 main.cpp — atlas + 빌보드 1장 정적 표시
a74d95a build(_MyApp_): activate apps/_MyApp_ + link SJH::engine
e40c589 [chore] : .gitignore 에 .worktrees/ 추가
9b8ee39 [refactor] : Mesh::CreateBox/CreatePlane 를 Geometry 위임으로 전환
94862a1 [feat] : SJH::Geometry 도형 데이터 생성기 어댑터 추가
7a218b6 [build] : engine/geometry.cpp 를 object 모듈 빌드에 편입
2c1fe38 [refactor] : Vertex 구조체를 vertex.h 로 분리 (GL 로더 비의존)
210c52e [fix] : EffekseerRendererGL 헤더 인클루드 경로 + _MyApp_ game_deps 링크
7b2a2c1 [fix] : EffekseerRendererGL 헤더 인클루드 경로 + _MyApp_ game_deps 링크
"""

def get_target_hashes():
    # 문자열에서 해시값만 추출한 뒤, 과거 커밋부터 수정해야 하므로 역순 정렬합니다.
    lines = [line.strip() for line in RAW_COMMITS.strip().split('\n') if line.strip()]
    lines.reverse()
    return [line.split()[0] for line in lines]

def is_target_hash(commit_hash, target_hashes):
    # Git 내부 해시 길이 포맷과 유연하게 매칭되도록 처리
    for h in target_hashes:
        if h.startswith(commit_hash) or commit_hash.startswith(h):
            return True
    return False

def main():
    # ---------------------------------------------------------------------------------
    # (A) 내부적으로 Git 시퀀스 에디터(Rebase 자동화 조작기)로 스크립트가 호출된 경우
    # ---------------------------------------------------------------------------------
    if os.environ.get("AUTO_REWORD_EDITOR") == "1":
        target_hashes = get_target_hashes()
        todo_file = sys.argv[1]
        
        with open(todo_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
        with open(todo_file, 'w', encoding='utf-8') as f:
            for line in lines:
                parts = line.split()
                # 리스트에 있는 해시면 'pick'을 'reword'(메시지 수정 모드)로 자동 변환
                if len(parts) >= 2 and parts[0] == 'pick':
                    commit_hash = parts[1]
                    if is_target_hash(commit_hash, target_hashes):
                        f.write(line.replace('pick', 'reword', 1))
                        continue
                f.write(line)
        sys.exit(0)

    # ---------------------------------------------------------------------------------
    # (B) 사용자가 직접 파이썬 스크립트를 실행했을 때 동작 (메인 진입점)
    # ---------------------------------------------------------------------------------
    print("=== VSCode 커밋 메시지 순차 수정 파이썬 스크립트 ===")
    print("제공된 해시 목록을 읽어, 과거 커밋부터 하나씩 VSCode 수정 창을 자동으로 띄웁니다.\n")
    
    target_hashes = get_target_hashes()
    if not target_hashes:
        print("해시 목록이 없습니다.")
        return
        
    # 시작점: 가장 과거 커밋의 부모(^)를 기준으로 설정
    base_commit = target_hashes[0] + "^"
    print(f"[실행] git rebase -i {base_commit}\n")
    
    # 환경변수를 조작하여 이 파이썬 스크립트를 Rebase 자동화기로, 
    # 실제 메시지 에디터를 VSCode(code --wait)로 강제 고정합니다.
    env = os.environ.copy()
    env["AUTO_REWORD_EDITOR"] = "1"
    env["GIT_SEQUENCE_EDITOR"] = f'"{sys.executable}" "{os.path.abspath(__file__)}"'
    env["GIT_EDITOR"] = "code --wait"
    
    # 해당 쉘 커맨드 실행: 파이썬이 모든 걸 통제하며 VSCode를 띄우기 시작합니다.
    result = subprocess.run(f"git rebase -i {base_commit}", shell=True, env=env)
    
    if result.returncode == 0:
        print("\n=== 모든 대상 커밋의 메시지 수정이 순차적으로 완료되었습니다! ===")
    else:
        print("\n[!] Rebase 중 중단이 발생했습니다. 터미널을 확인해주세요.")

if __name__ == "__main__":
    main()
