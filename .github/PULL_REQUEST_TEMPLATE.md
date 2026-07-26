<!--
  이 템플릿은 .claude/skills/code-design-review-lenses/SKILL.md 의 5렌즈 + Self-review gate 를
  PR 작성 시점에 반영하기 위한 것이다. 스킬이 정본(SSOT) — 렌즈 명칭/문구가 스킬과 어긋나면 스킬 쪽을 따른다.
-->

## Summary

<!-- 이 PR 이 무엇을 바꾸는지, 왜 바꾸는지 자유 기술 -->

-

## Test plan

<!-- 어떻게 검증했는지 체크/기술. 해당 없는 항목은 지우거나 "N/A — 이유" 로 남길 것 -->

- [ ] 빌드 확인 (`cmake --build --preset ninja --target _MyApp_` 등)
- [ ] `ctest --test-dir build_ninja --output-on-failure` (해당 시 — smoke/gpu/CPU 단위. 골든은 미포함)
- [ ] 골든 이미지 비교 통과 (렌더링 출력이 바뀌는 변경이면 **필수** — 프리셋 `ninja-golden` 전유. Release 검증은 `ninja-release-golden`)
  ```bash
  cmake --preset ninja-golden && cmake --build --preset ninja-golden --target tests
  ctest --test-dir build_ninja-golden -R "골든" --output-on-failure
  ```
  - [ ] 골든 REF(`test/golden/*.png`)를 갱신했다면 **이 PR 안에** 포함했는가 (원인 커밋과 분리 금지 — 분리하면 원인 커밋이 게이트 RED 로 남는다)
- [ ] 육안 검증 (GUI 실행 확인)
- [ ] 기타:

## Design review (5 lenses)

<!-- code-design-review-lenses 스킬의 5렌즈. 구조 변경이 없는 사소한 PR 이면 해당 없음으로 표시 가능 -->

- [ ] **Lens ① Ownership model** — 소유권이 명확한가 (단일 소유자 + raw reference, 또는 `shared_ptr`/`new`/불명확 소유권 남용 없는가; 핸들 보유 클래스는 복사/대입 차단하는가)
- [ ] **Lens ② Coupling** — 의존이 단방향인가 (A→B 인데 B→A 도 필요한가; forward declaration 으로 끊을 수 있는 사이클 없는가; 책임 밖 타입을 알게 된 클래스 없는가; 추상이 구체를 아는 역전 없는가)
- [ ] **Lens ③ ODR / header hygiene** — 헤더 가드 일관성, 헤더에 non-inline 전역 정의 없는가, header-only inline 함수가 적절한가
- [ ] **Lens ④ SOLID / separation of responsibility** — S/O/L/I/D 각 원칙 위반 없는가 (특히 단일 책임·확장점·인터페이스 비대·구체 클래스 직접 의존)
- [ ] **Lens ⑤ Consistency** — 명명/컨벤션/패턴이 기존 코드베이스와 일관되는가 (`personal-naming-conventions`, 기존 모듈 패턴 재사용)

<!-- 각 체크에 대해 필요 시 file:line 근거 + 💚/💛/🚨/💭 짧게 남길 것 -->

## Self-review gate

<!--
  code-design-review-lenses SKILL.md 의 "Self-review gate — apply the lenses to your OWN proposal" 반영.
  구조 변경 제안은 검증 비대칭(자기 제안은 관성적으로 통과) 위험이 있으므로, 보내기 전에
  타인의 PR 처럼 다시 읽고 최소 Lens ①/④ 를 자기 제안에 역적용해야 한다.
-->

- [ ] 이 PR 은 구조 변경(새 클래스/모듈/인터페이스/상속)을 제안하는가?
- [ ] (위가 예라면) 렌즈 ① (Ownership) · 렌즈 ④ (SOLID/SRP) 를 내 제안에 자기 역적용했는가?

<!-- 자유 기술: 왜 이 구조가 정당한지 / 이 데이터·책임을 기존에 누가 소유했는지 grep 으로 확인했는지 -->

Self-review verdict:

-
