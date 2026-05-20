# PHASE1_RUNBOOK — 1주 PoC 실행 절차

> 사용자의 C++ OpenGL 게임 클라이언트에서 **단일 렌더 패스 1개**를 5-에이전트 운영 분리로 안전하게 리팩토링하는 1주 PoC.
>
> Day 1-2가 가장 중요하다. 골든 이미지의 결정성 확보가 모든 후속 단계의 전제다.

---

## Day 1 (월) — 환경 + 첫 골든

### 오전: 환경 점검

```bash
# 1. Mesa llvmpipe 작동 확인
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe glxinfo -B | grep "OpenGL renderer"
# 기대: "OpenGL renderer string: llvmpipe (LLVM ...)"

# 2. NVIDIA FLIP 설치 (https://github.com/NVlabs/flip)
git clone https://github.com/NVlabs/flip
cd flip && mkdir build && cd build && cmake .. && make -j

# 3. clang-tidy / cppcheck 설치
which clang-tidy cppcheck

# 4. (선택) mull 설치 (mutation testing). 실패해도 Day 4까지 진행 가능
```

체크리스트:
- [ ] llvmpipe로 게임 클라이언트가 실행되는가? (5fps여도 OK)
- [ ] FLIP 명령이 두 PNG의 차이를 계산하는가?
- [ ] clang-tidy가 게임 코드 1개 파일에 동작하는가?

### 오후: 리팩토링 대상 렌더 패스 선정

**선정 기준** (작은 것부터):
- ✅ 외부 의존성이 적다 (다른 패스 영향 X)
- ✅ 결정적으로 실행 가능 (시간/RNG 의존성 없음)
- ✅ 입출력이 명확 (입력 텍스처·UBO → 출력 텍스처)
- ❌ 동적 그림자, post-process chain, particle system은 처음엔 피한다

**좋은 후보 예시**:
- Tone mapping pass (단일 입력 → LUT → 단일 출력)
- Bloom blur pass (이미 격리된 effect)
- Skybox draw pass (단순)

선정 후:
```bash
echo "TARGET_PASS=src/render/passes/<your_pass>.cpp" > .claude/poc.env
```

---

## Day 2 (화) — 결정적 시드 시나리오 + 골든 이미지

### Mock RHI 인터페이스 정의 (사용자 직접)

이 부분은 본 PoC가 **만들어주지 못했다** — 사용자 게임 엔진의 RHI 구조를 모르기 때문. 다음 가이드에 따라 직접 정의:

```cpp
// include/IRHIDevice.h (예시 — 사용자 환경에 맞춰 변경)
class IRHIDevice {
public:
    virtual ~IRHIDevice() = default;
    virtual TextureHandle CreateTexture(const TextureDesc&) = 0;
    virtual void BindTexture(uint32_t slot, TextureHandle) = 0;
    virtual ShaderHandle CompileShader(const std::string& src, ShaderStage) = 0;
    // ... 본 패스에서 호출하는 모든 gl* 함수에 대응
};

class OpenGLRHIDevice : public IRHIDevice {
    // 실제 gl* 호출하는 구현체 (이미 있을 수 있음)
};

class MockRHIDevice : public IRHIDevice {
    // 호출 시퀀스를 std::vector<RHICall>에 기록만
    std::vector<RHICall> recorded_calls;
};
```

### 결정적 시드 시나리오 작성

```cpp
// tests/scenarios/<pass_name>_baseline.cpp
void RunBaselineScenario() {
    Camera cam{
        .position = {0, 5, 10},
        .target = {0, 0, 0},
        .fov = 60.0f
    };
    SetGlobalTime(0.0);
    SetRNGSeed(42);
    LoadFixedScene("test_assets/baseline.scene");

    for (int i = 0; i < 100; ++i) {
        AdvanceFrame(/*dt=*/1.0f / 60.0f);
    }

    // glFinish 또는 fence 동기화 후 readback
    glFinish();
    SaveColorAttachmentToPNG("/tmp/baseline.png");
}
```

### 골든 이미지 캡처 + 5회 재현성 검증

```bash
# 5번 실행해서 모두 동일한지 확인
for i in 1 2 3 4 5; do
    ./scripts/run_headless.sh baseline /tmp/baseline_run${i}.png
done

# 모든 비교 결과가 wm=0이어야 함
for i in 2 3 4 5; do
    flip --reference /tmp/baseline_run1.png \
         --test /tmp/baseline_run${i}.png \
         --basename /tmp/det_check_${i}
done
```

**5회 모두 동일하면 골든 채택**:
```bash
mv /tmp/baseline_run1.png tests/golden/baseline.png
git add tests/golden/baseline.png
git commit -m "chore(golden): add baseline for <pass_name>"
```

**5회 결과가 다르면**: 비결정성 원인을 찾는다. 흔한 원인:
- 시간 의존 코드 (`std::chrono` 호출)
- 정렬되지 않은 컨테이너 순회
- 부동소수점 누적 오차 (multi-threading order)
- 드라이버 자체의 비결정성 (llvmpipe인데 그러면 mesa 버전 확인)

---

## Day 3 (수) — 첫 리팩토링 시도 (작은 것)

### 명세 작성

GitHub issue 또는 `docs/refactor_specs/<pass_name>.md`:

```markdown
# Refactor Spec: <pass_name>

## Goal
GPU resource binding 로직을 별도 함수 `BindShadowResources()`로 추출

## Measurable Criteria
- 추출된 함수 시그니처가 정확히 1개 추가됨
- 원본 함수의 AST node count delta < 30%
- 외부 진입점 변경 없음
- 골든 이미지 FLIP weighted median ≤ 0.05

## Out of Scope
- 셰이더 코드 변경 X
- 다른 패스 영향 X
- API surface 변경 X
```

### Claude Code 실행

```bash
claude
```

명령:
```
/refactor-pass src/render/passes/<your_pass>.cpp "GPU resource binding 로직을 BindShadowResources() 함수로 추출"
```

5단계 자동 진행:
1. `render-architect`: 의존성 분석 (5분)
2. **사람 검수 게이트**: 분석 보고서 30초 검토
3. `render-refactorer`: worktree에서 리팩토링 시도 (10-30분)
4. `render-test-debug`: 골든 비교 (5분)
5. `render-quality-gate`: clang-tidy + (선택) mutation (10분)
6. `render-pm`: 최종 결정

### Day 3 끝 체크리스트
- [ ] 첫 리팩토링이 PASS로 머지되었는가? (실패해도 OK — Day 5에 회고)
- [ ] FLIP 결과 weighted median이 임계값 이하인가?
- [ ] PR 코멘트에 mutation score가 기록되었는가?

---

## Day 4 (목) — 두 번째 리팩토링 + 게이트 캘리브레이션

Day 3와 동일 절차로 두 번째 작은 리팩토링. 다른 패턴:
- "변수 이름 변경" 같은 더 작은 것
- 또는 "if-else 체인을 switch로 변경"

**Day 3·4 데이터로 임계값 캘리브레이션**:

```bash
# FLIP 임계값이 너무 엄격한가?
grep "weighted median" /tmp/flip_results/*.csv | sort

# Mutation Score가 60%에 도달 가능한가?
grep "mutation score" /tmp/quality_gate_*.json
```

필요시 `CLAUDE.md` §B.1 또는 `docs/METRICS.md` 임계값 조정.

---

## Day 5 (금) — 회고 + 시스템 프롬프트 미세 조정

### `docs/AGENTS_GUIDE.md` §3의 6가지 질문에 답하기

각 에이전트 출력 로그를 검토:
- `~/.claude/projects/<your_project>/conversations/`

체크:
- 어느 에이전트가 가장 자주 실패했나?
- iteration budget을 소진한 케이스가 있는가?
- "확인 필요"라고 안 하고 환각으로 진행한 케이스가 있는가?

### 시스템 프롬프트 보강

발견된 실패 모드를 해당 에이전트의 "절대 금지" 섹션에 추가:

```markdown
## 절대 금지

- (기존 항목들)
- **<발견된 실패 모드>**: <구체적 트리거 조건>
```

---

## Day 6-7 (주말 또는 다음 주 초) — 두 패스 시도 + Phase 2 게이트 점검

### 더 복잡한 패스에 적용
Day 1에서 제외했던 약간 더 복잡한 패스 1개에 적용. 예: post-process chain의 단일 단계.

### Phase 2 진입 게이트 (모두 충족 시 Phase 2 시작)
- [ ] 최소 3개의 리팩토링이 자동으로 PASS 머지됐다
- [ ] FLIP 임계값이 캘리브레이션됐다 (현재 게임에 맞게)
- [ ] Mutation Score가 60% 이상 달성된 경험이 있다
- [ ] iteration budget 소진 케이스가 < 30%
- [ ] `docs/AGENTS_GUIDE.md` §3 6가지 질문에 답을 작성했다

---

## 자주 발생할 문제와 대응

### "골든 이미지가 5회 재현성 통과 못함"
원인: 비결정성. 다음 순서로 의심:
1. 시간 의존 코드 → `frame_index` 매개변수로 대체
2. 정렬되지 않은 컨테이너 → `std::sort` 후 순회
3. 멀티스레드 → 단일 스레드 baseline 시나리오 별도 작성

### "render-refactorer가 같은 파일을 3번 수정함"
A §6.3의 oscillation. 즉시 사람 escalate. 명세 모호성 의심:
- 측정 가능 기준이 명확한가?
- 두 기준이 서로 충돌하는가?

### "PR 비용이 $10 cap 초과"
- iteration budget 6→3으로 축소
- 또는 패스를 더 작은 단위로 분해

### "FLIP 임계값에 절대 통과 못함"
- 게임이 동적 효과가 많아 결정성 자체가 어려운 케이스
- baseline scenario를 더 정적인 것으로 변경 (조명 정적, 카메라 정지)

---

## Phase 1 끝나면 outputs

`docs/phase1_results.md`에 작성:
- 시도한 리팩토링 N개
- PASS / WARN / REVERT 비율
- 평균 비용 / 시간
- 발견된 안티패턴
- 시스템 프롬프트 변경 이력
