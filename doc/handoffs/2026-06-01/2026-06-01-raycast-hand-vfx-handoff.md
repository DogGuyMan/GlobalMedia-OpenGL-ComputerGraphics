# Handoff — 마우스 Raycast + 조준 연결(발사/회전/손) + Effekseer VFX 테스트 (2026-06-01)

> 대상: `_MyApp_`. 브랜치 `game/module/ingame/temp`.
> 본 문서는 **Claude(에이전트)가 직접 손 댄 작업**의 요약 + 인계용. 같은 세션에서 사용자가 동시 구현한
> 부분(PlayerController 최종 리팩토링, PlayerActor.cpp, Weapon, PlayerHand 등)은 §4 에 출처 구분해 명시.
> 빌드 상태: **`cmake --build --preset ninja --target _MyApp_` 통과** (실행 검증은 미완 — §5).

---

## 1. 사용한 Spec / Plan

| 문서 | 위치 | 내용 |
|---|---|---|
| **조준 벡터 연결 Spec** (정본) | `doc/superpowers/specs/2026-06-01-aim-vector-connections-design.md` ⚠ gitignore(로컬) | brainstorming 으로 작성. D1 연속 조준 / D2 플레이어 회전(논리 Y facing + uFlipX) / D3 발사 = `Weapon::UseWeapon` 경유 / D4 손 = player 자식 상속 궤도. 좌표 매핑·영향 파일·비목표 포함. |
| PoliceTape 벽 + Ground 픽킹 Handoff | `doc/design/2026-06-01-stage-policetape-ground-handoff.md` | 직전 작업 기록. box2d Ground body 제거 → 수학 ray-plane 픽킹 확정. |

- 별도 *writing-plans* 산출물은 없음 — spec review 단계에서 사용자가 직접 구현에 착수(아래 §4).
- 좌표 규칙(불변): `box2d → world = (x, h, -y)` → **world.z = -box2d.y**. 발사 box2d dir = `(aim.x, -aim.z)`. facing Y각 = `degrees(atan2(-dir.x, -dir.z))` (Transform.EulerRot = degree).

---

## 2. 에이전트가 *직접 생성* 한 파일

| 파일 | 내용 |
|---|---|
| `apps/_MyApp_/resources/shaders/transparent.vs` | 반투명 unlit VS — `uvScale` 타일링 + `uTime*uScrollSpeed` U 스크롤 |
| `apps/_MyApp_/resources/shaders/transparent.fs` | 반투명 unlit FS — `emissive` 샘플 + `color.a<0.1` 알파 컷오프 (※ 이후 사용자가 출력부 미세 수정) |
| `apps/_MyApp_/src/Stage/Components/MaterialTimeComponent.h` | `MaterialTime` — 누적 dt 를 머티리얼 `uTime` 에 매 프레임 기록 (테이프 스크롤 시간원) |
| `apps/_MyApp_/src/UI/VfxSpawnLayer.h` | **VFX 테스트 드롭다운 레이어** — `ImGui::Combo` 로 로드된 이펙트 선택 + `GetSelectedEffect()` |
| `doc/design/2026-06-01-stage-policetape-ground-handoff.md` | 벽/픽킹 핸드오프 |
| `doc/superpowers/specs/2026-06-01-aim-vector-connections-design.md` | 조준 연결 spec (gitignore) |

## 3. 에이전트가 *직접 수정* 한 파일

| 파일 | 수정 내용 |
|---|---|
| `apps/_MyApp_/src/Stage/StageBuilder.cpp` | pickup 시각 actor 제거 → 벽 = PoliceTape 반투명 펜스 / `spawnWall` (yRot 통합, half 자동 도출) / MaterialTime 부착 |
| `apps/_MyApp_/src/InputHandler/PlayerController.h` | `SetWorldCamera` + `mCamera`, 조준 멤버/getter, `SetGroundClickCallback` 도입 (※ 이후 사용자가 `UpdateAim`/`OnFirePressed` 로 추가 리팩토링) |
| `apps/_MyApp_/src/InputHandler/PlayerController.cpp` | 마우스→Ground **수학 raycast**, 디버그 노란 마커 스폰, 조준 추출, `GLFW_INCLUDE_NONE`(맨 위), 좌클릭→ground-click 콜백 (※ 사용자가 `UpdateAim`/`OnFirePressed` 로 흡수·확장) |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.h` | `ControllerCfg.camera` 추가 + `SetWorldCamera` wiring (※ 이후 사용자가 `WeaponCfg`/`SpriteCfg` 추가 + 정의를 `.cpp` 로 이전) |
| `apps/_MyApp_/main.cpp` | World 카메라 주입(`pac.controller.camera`) / **VFX 6종 로드** / **VfxSpawnLayer push** + `mVfxLayer` / 좌클릭 `SetGroundClickCallback` → 선택 이펙트 `SpawnVfxInstance` |

> 일시 추가 후 **되돌린** 것(현재 코드에 없음): `wall_factory.h::CreateGroundActor`, `PhysicsLayer.h::PhysicsLayer::Ground`,
> `StageBuilder` Ground 스폰 — box2d Ground body 폐기(수학 raycast 채택) 결정으로 원복.

---

## 4. 같은 세션에서 *사용자가 직접* 구현한 부분 (출처 구분 — 에이전트 작업 아님)

에이전트가 만든 *기반*(카메라 주입 + 수학 raycast + 조준 추출 + ground-click 콜백 + spec) 위에 사용자가 D1~D4 본구현을 올림:

- `PlayerController.{h,cpp}` — `UpdateAim()`(연속 조준, 매 프레임) + `OnFirePressed()`(Weapon 발사 + onFire + 마커 + ground-click 콜백) + 플레이어 회전(EulerRot.Y + `SpriteRenderer.flipX` lazy 캐시).
- `Entity/Components/WeaponComponents.{h,cpp}` — `Weapon::UseWeapon(box2dForward)` 실제 총알 스폰(`CreateBulletActor`, world→box2d) + `SetWorld`.
- `Entity/Player/PlayerActor.cpp` (신규) — `CreatePlayerActor` 정의를 헤더→`.cpp` 이전 + **Weapon 부착** + **4-레이어 directional 스프라이트 합성**.
- `Entity/Player/PlayerHand.{h,cpp}` — `PlayerHands`(OnEnter 에서 자식 Hand actor 2개 생성) + `PlayerSingleHand`(spread/radius/yOffset, 부모 Y facing 상속 궤도). `PlayerBuilder` 가 player 에 `PlayerHands` 부착.
- `Bootstrap/PlayerBuilder.{cpp,h}` — `pac.weapon`(damage/world) 세팅 + PlayerHands wiring.
- `main.cpp` — 일반 4x4 역행렬 `Mat4Inverse`(MESA 정통) 추가(향후 정밀 unprojection 용).

즉 **조준 연결 D1~D4 는 코드상 완료**(빌드됨). 에이전트 기여 = 기반 + VFX 테스트, 사용자 기여 = 조준/발사/손/스프라이트 본구현.

---

## 5. 현재 상태 / 미완 (Open)

1. **실행 검증 0** — 빌드만 통과. 실제 실행해 (조준 방향/발사/손 궤도/카메라 basis 정합/VFX 소환) 확인 필요. 특히
   카메라 basis 를 owner WorldMatrix 컬럼에서 뽑으므로(고정 −30° pitch 전제) 마커가 클릭 지점과 어긋나면
   `GetViewMatrix()` 기반으로 전환.
2. **VFX 텍스처 누락 (블로커)** — `resources/vfx/` 에 png 0개. 6개 .efk 가 참조하는 텍스처 없으면 이펙트가 *안 보임*.
   필요 배치: laser/orbital/summon → `resources/vfx/Texture/`, dust → `resources/vfx/Pierre01/Texture/`,
   slash → `resources/vfx/particles/`, **hit → `resources/Texture/` + `resources/Material/`(vfx 의 상위)**.
   (git 에서 `vfx/01_AndrewFM01/Texture/*` 삭제된 것들과 겹침 — 원본 복원 필요.)
3. **좌클릭이 발사+VFX소환+마커 동시 발생** — VFX만 테스트하려면 분리(별 모드/버튼) 선택.

## 6. Gotchas

- **GLFW_INCLUDE_NONE** 은 `PlayerController.cpp` *첫 include 이전*. mouse_input.h 가 GLFW 를 끌어와 gl3w 와 `PFNGL*` 충돌 방지.
- **clangd stale** — 헤더 시그니처/멤버 변경 직후 "No member/Too many args" 가 잠깐 뜸. 빌드가 정답.
- **증분 빌드 stale lib** — 동시 편집 시 ninja 가 의존 라이브러리(예: `libmyapp_entity.a`)를 재컴파일 안 해 *링크* 에서 미정의 심볼이 날 수 있음 → 해당 `.cpp` 를 `touch` 후 재빌드.
- VFX 단발 소환은 `Spawns::SpawnVfxInstance(*mFxRoot, &VFX(), effect, pos)` (auto-despawn + sweep). 로드는 `reg.CreateEffect(manager, key, u"path")`.
- `doc/` 는 gitignore(로컬 전용). 핸드오프/추적 문서는 `doc/`(단수).

## 7. 빌드 / 실행
```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
# WASD 이동 / 마우스로 조준(손·flip) / 좌클릭 = 발사+VFX소환+마커
# "VFX Test" 창에서 이펙트 선택 후 바닥 클릭
```
