# Material & Texture 리팩토링 로드맵

> 상용 게임 엔진 **sbox** (Facepunch, Source 2 기반) 의 `Texture.Load.cs` / `Texture.Read.cs` 를 분석해서
> 그 디자인 철학을 `exercise6` 에 단계적으로 적용하는 가이드.
>
> 목표: **"내가 직접 만들어 보면서" 상용 엔진 수준의 Texture / Material 관리 구조를 체득** 하는 것.
>
> 각 Step 은 독립 커밋 단위로 가능하며, 이전 단계가 동작하는 상태에서 다음으로 나아간다.

---

## 📖 왜 이 로드맵이 필요한가

현재 `exercise6` 의 텍스처 관리는 이런 문제들을 안고 있다:

| 문제 | 현재 코드 | 증상 |
|------|----------|------|
| 중복 로드 | `AddTexture(path)` 호출마다 `stbi_load` + `glGenTextures` | 같은 파일 여러 번 쓰면 GPU 메모리 낭비 |
| 암묵적 슬롯 계약 | `mTextureAddrs[0]=tex1`, `[1+f]=tex2` 관습 | 호출 누락 시 `operator[]` OOB → UB/SEGV |
| GL handle 노출 | `vector<GLuint>` 직접 저장 | 소유권 불명확, double-delete 위험 |
| 실패 무음 | `stbi_load(null)` 이어도 `glTexImage2D(null)` 진행 | 로드 실패를 인지할 수 없음 |
| Draw 하드코딩 | `Draw()` 가 큐브 6면 루프 가정 | 다른 메쉬에서 "부채꼴" 증상 (Disk 사례) |
| 타입 매칭 없음 | `glUniformMatrix4fv(vec2)` 같은 실수가 어디서나 발생 가능 | 텍스처 단색, UV 애니메이션 무반응 |

이 중 대다수는 **"구조가 먼저 설계되면 애초에 발생 불가능"** 한 범주다. sbox 의 패턴이 이를 어떻게 차단하는지 보고, 그 구조를 직접 이식한다.

---

## 🎯 5가지 핵심 철학 (sbox 에서 추출)

리팩토링의 모든 결정은 이 다섯 원칙으로 되돌아간다. 단계별로 의심이 생기면 이걸 다시 읽어볼 것.

### 1️⃣ Single Funnel, Multiple Façades
공개 API 는 용도별로 여러 개이되, **내부 구현은 단일 진입점**. 버그 수정·캐시·로깅을 한 곳만 건드리면 전체에 파급.

> sbox 예: `Load()`, `LoadFromFileSystem()`, `LoadAvatar()`, `LoadAsync()` 가 모두 `LoadInternal()` 로 귀결.

### 2️⃣ Handles are Accidents, Identities are Truth
GL handle (`GLuint`) 은 구현 세부사항. **진짜 정체성은 "파일 경로 + 메타데이터"**. 캐시 키는 identity, 조회 결과는 handle wrapper.

### 3️⃣ Make Ownership Explicit, Make Lifetime a Contract
- 복사 금지 + `shared_ptr` 공유
- async 콜백의 `doneWithData()` 같은 **명시적 수명 신호**
- `Span<T>` 의 유효 범위를 주석으로 박제

### 4️⃣ Strategy over Switch
새 loader (SVG, URL, Avatar 등) 추가 시 **기존 코드를 건드리지 않음**. `IsAppropriate + Load` 인터페이스를 구현하고 dispatcher 에 등록만.

### 5️⃣ Defensive Validation is the First Line Against UB
`unsafe` / GL 호출 전에 **모든 경계 조건을 사전 차단**. 특히 `operator[]`, 포인터 캐스팅, 포맷 변환 구간.

---

## 🗺️ 전체 로드맵 개요

| Step | 이름 | 원리 | 예상 난이도 | 커밋 단위 |
|------|------|------|------------|----------|
| **1** | `Texture` 클래스 분리 | 원리 ②③ — GL handle 캡슐화 + 소유권 명시 | 🟢 쉬움 | 독립 |
| **2** | `TextureCache::Load` | 원리 ① — 단일 funnel + 중복 제거 | 🟢 쉬움 | Step 1 선행 |
| **3** | `Material` + `TextureSlot` | 원리 ③④ — Sampler 이름 명시, uniform 집중 | 🟡 중간 | Step 1~2 선행 |
| **4** | `ModelBase::Draw` 범용화 | 원리 ⑤ — 특정 메쉬 상수 제거 | 🟡 중간 | Step 3 선행 |
| **5** | Hot Reload | 원리 ② — 핸들 뒤 실체만 교체 | 🔵 어려움 | 선택 |
| **6** | `Texture::Load` 비동기 | 원리 ③ — cancellation token 캡처 | 🔵 어려움 | 선택 |
| **7** | `GetPixels<T>` | 원리 ⑤ — 제네릭 + 방어 검증 | ⚪ 선택 | 독립 |

**권장 진행**: **Step 1 → 2 → 3 → 4** 까지만 완료해도 현재 `exercise6` 의 텍스처 관련 버그가 **구조적으로 재발 불가능** 한 상태가 된다. 5~7 은 학습 목적의 선택 과제.

---

## 🪜 Step 1 — `Texture` 클래스 분리

### 목표

GL texture handle 을 **오직 `Texture` 객체만 소유**하게 만든다. 복사는 금지하고 이동만 허용해서 double-delete 를 원천 차단한다.

### 왜 이게 먼저인가

이후 Step 2~4 가 모두 `shared_ptr<Texture>` 를 기반으로 동작한다. 기본 벽돌부터 쌓아야 나머지가 의미 있다.

### 설계 초안

네임스페이스 `exercise6` 안에 새 클래스를 선언:

```cpp
class Texture
{
  private:
    GLuint mAddr = 0;
    int mWidth = 0;
    int mHeight = 0;
    int mChannels = 0;
    std::string mPath;

  public:
    Texture() = default;

    // 🔒 복사 금지 — GL handle 소유 타입은 복사되면 안 됨
    //    (STUDY_NOTE 의 Exercise6 Priority 1-3 참조)
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // ✅ 이동만 허용 — source 는 handle=0 으로 무효화
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;

    ~Texture();     // glDeleteTextures — 여기서만 해제

    // 📦 로드 — 실패 시 false 반환 (exception 대신)
    bool LoadFromFile(const std::string& path);

    // 📖 읽기 전용 접근자
    GLuint             GetAddr()     const { return mAddr; }
    int                GetWidth()    const { return mWidth; }
    int                GetHeight()   const { return mHeight; }
    int                GetChannels() const { return mChannels; }
    const std::string& GetPath()     const { return mPath; }
    bool               IsValid()     const { return mAddr != 0; }
};
```

### 구현 체크리스트

- [ ] **이동 생성자**: source 의 `mAddr` 를 `0` 으로 세팅한다. 안 그러면 source 소멸 시 우리 handle 이 파괴됨.
- [ ] **이동 할당**: self-assignment 체크 + 기존 내 handle 해제 후 이동.
- [ ] **소멸자**: `mAddr != 0` 일 때만 `glDeleteTextures` 호출 (0 은 no-op 이라 사실 체크 안 해도 되지만, 습관으로).
- [ ] **`LoadFromFile`**:
  1. `stbi_load` 로 이미지 로드
  2. **실패 시 `std::cerr` 로 경고** + `false` 반환 (실패 무음 금지)
  3. 성공 시 `glGenTextures` → `glBindTexture` → `glTexImage2D` → `glGenerateMipmap`
  4. `glTexParameteri` 설정
  5. `stbi_image_free`
  6. 멤버 변수 저장 (`mAddr`, `mWidth`, `mHeight`, `mChannels`, `mPath`)
- [ ] **GL 포맷 판정**: `nrChannels == 3 ? GL_RGB : GL_RGBA` (stbi 의 채널 수를 존중)
- [ ] **빌드 성공** 확인

### 검증

```cpp
// 임시 테스트
{
    Texture t;
    assert(!t.IsValid());
    bool ok = t.LoadFromFile("./textures/container.jpg");
    assert(ok && t.IsValid());
    assert(t.GetWidth() > 0);
}
// 이 시점에 GL 핸들이 자동 해제됐는지 확인 (GL 디버깅 도구로)
```

### ⚠️ 함정

- **기존 `ModelBase::AddTexture` 를 아직 건드리지 말 것.** Step 1 은 `Texture` 클래스만 도입. 호출부는 Step 2~3 에서 정리한다.
- **`#include <stb_image.h>` 가 이미 main.cpp 에 `STB_IMAGE_IMPLEMENTATION` 로 정의돼 있으니**, 같은 파일 안에서만 stbi 를 호출하거나, 별도 .cpp 로 분리.
- **복사 대입 연산자도 `= delete`** 잊지 말 것. 복사 생성자만 막아봤자 대입으로 새어들어감.

---

## 🪜 Step 2 — `TextureCache::Load` 로 중복 제거

### 목표

같은 파일 경로를 여러 번 로드해도 **GPU 메모리에는 한 번만** 올라가게 만든다.

### 왜 필요한가

현재는 model 두 개가 `container.jpg` 를 쓰면 VRAM 에 **container.jpg 가 두 벌** 존재한다. sbox 의 `NativeResourceCache` 는 이걸 instance ID 로 차단한다. 우리는 파일 경로를 key 로 쓴다.

### 설계 초안

```cpp
class TextureCache
{
  private:
    // weak_ptr 로 저장 → 아무도 사용 안 하면 자동으로 해제됨
    static std::unordered_map<std::string, std::weak_ptr<Texture>> sCache;

  public:
    // 🎯 Single Funnel — 모든 텍스처 로드는 이 함수 하나를 거쳐야 함
    static std::shared_ptr<Texture> Load(const std::string& path);

  private:
    // 경로 정규화 — 같은 파일의 다른 표기를 하나로 취급
    // 예: "./textures/a.jpg" == "textures/a.jpg" == "TEXTURES/A.JPG" (OS 따라)
    static std::string NormalizePath(const std::string& path);
};
```

### `Load` 구현 순서

```cpp
std::shared_ptr<Texture> TextureCache::Load(const std::string& path)
{
    // 1️⃣ 경로 정규화
    auto normalized = NormalizePath(path);

    // 2️⃣ 캐시 확인 — weak_ptr 이 살아있으면 재사용
    auto it = sCache.find(normalized);
    if (it != sCache.end())
    {
        if (auto cached = it->second.lock())
            return cached;               // 캐시 히트

        // 죽은 weak_ptr 정리 (sbox 의 "IsValid 체크 후 evict" 패턴)
        sCache.erase(it);
    }

    // 3️⃣ 새로 로드
    auto tex = std::make_shared<Texture>();
    if (!tex->LoadFromFile(normalized))
    {
        std::cerr << "TextureCache: load failed: " << normalized << std::endl;
        return nullptr;
    }

    // 4️⃣ 캐시 등록 후 반환
    sCache[normalized] = tex;
    return tex;
}
```

### 구현 체크리스트

- [ ] `sCache` 를 **namespace scope 나 static 멤버** 로 정의 — 정의 라인 필수 (`std::unordered_map<...> TextureCache::sCache;`)
- [ ] `NormalizePath`: 최소한 앞쪽 `"./"` 제거, 소문자 변환(OS 에 따라), 중복 슬래시 정리 정도
- [ ] `weak_ptr` 사용 이유 **이해** — `shared_ptr` 로 저장하면 캐시가 영원히 참조 카운트를 잡고 있어서 메모리 해제 안 됨. `weak_ptr` 은 "누가 쓰고 있으면 빌려오고, 아무도 안 쓰면 자연스럽게 죽는다" 를 구현.
- [ ] `Load` 반환값이 `nullptr` 일 때 호출자가 이를 처리할 수 있는지 고려 (Material 에서 체크 필요)
- [ ] 같은 파일을 2번 `Load()` 호출했을 때 **반환된 포인터가 동일한지** 검증 (`ptr1.get() == ptr2.get()`)

### 검증

```cpp
auto t1 = TextureCache::Load("./textures/container.jpg");
auto t2 = TextureCache::Load("./textures/container.jpg");
assert(t1.get() == t2.get());        // 같은 객체
assert(t1.use_count() >= 2);          // 두 곳에서 참조

// 존재하지 않는 파일
auto t3 = TextureCache::Load("./textures/nonexistent.jpg");
assert(t3 == nullptr);                // 실패 시 null (STUDY_NOTE 2-3 교훈)
```

### ⚠️ 함정

- **정적 초기화 순서 문제**: `sCache` 를 `static` 멤버로 두면 OK 지만, 다른 static 객체가 `Load()` 를 호출하면 초기화 순서가 꼬일 수 있음. "Construct On First Use" 이디엄으로 `static` 함수 내부에 두는 것도 방법:
  ```cpp
  static auto& GetCache() {
      static std::unordered_map<std::string, std::weak_ptr<Texture>> cache;
      return cache;
  }
  ```
- **GL 컨텍스트 의존성**: `Load` 내부의 `Texture::LoadFromFile` 이 GL 호출을 하므로, **반드시 GL 컨텍스트가 활성화된 이후** (`startup()` 내) 에서만 호출 가능. `static` 변수 초기화에 `Load` 를 박으면 안 됨 (Exercise6 노트 1-2 참조 — GL 객체 값 멤버 함정).

---

## 📚 보충 개념 — Multi-texturing & Texture Unit Persistence

> Step 3 으로 넘어가기 전에 이 개념을 확실히 잡아두면 Material 설계가 자연스러워진다.

### "Draw call 1회 = 텍스처 1개" 가 아니다

흔한 오해: "7개 텍스처를 쓰려면 draw 도 7번 해야지". 실제는 전혀 다르다.

**진실**: OpenGL 의 texture unit (`GL_TEXTURE0`, `GL_TEXTURE1`, ...) 은 **독립된 글로벌 슬롯**이고, **한 draw call 의 shader invocation 은 여러 unit 에서 동시에 샘플링** 할 수 있다. `uniform sampler2D tex1, tex2;` 가 있으면 그 shader 는 매 픽셀마다 두 텍스처를 모두 읽는다.

### Texture Unit 의 글로벌 지속성

```
OpenGL Context State
├── Unit 0  →  [ 무엇이 꽂혀있음 ]   ← 새로 bind 하기 전까지 영구 유지
├── Unit 1  →  [ 무엇이 꽂혀있음 ]
├── Unit 2  →  [ 무엇이 꽂혀있음 ]
├── ...
```

- **bind = 꽂는다**. `glBindTexture(GL_TEXTURE_2D, tex)` 는 **현재 active unit 슬롯**에 texture 를 꽂는 동작.
- **unbind 자체가 없다**. 다른 걸로 덮어쓰거나 프로그램이 종료될 때까지 슬롯에 그대로 남는다.
- **`glActiveTexture(GL_TEXTUREn)`** 는 "이후 `glBindTexture` 호출이 n 번 슬롯에 꽂힐 것" 을 지정하는 **포인터** 같은 역할.

### 실제 exercise6 Cube::Draw 추적

```cpp
// 루프 밖 — unit 0 에 container 를 한 번만 꽂음
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);   // container

// 루프 안 — unit 1 만 매 iteration 마다 교체
for (int f = 0; f < 6; f++) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);   // side[f]
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(f * 6 * sizeof(GLuint)));
}
```

매 draw 시점의 state:

| draw # | face | Unit 0 (tex1) | Unit 1 (tex2) |
|--------|------|---------------|---------------|
| 1 | -Z | container | side1 |
| 2 | +X | container | side2 |
| 3 | +Z | container | side3 |
| 4 | -X | container | side4 |
| 5 | -Y | container | side5 |
| 6 | +Y | container | side6 |

**container 가 모든 draw 에 같이 있는 이유**: 아무도 unit 0 을 다시 건드리지 않았기 때문. "한 번 꽂으면 그대로" 원칙.

### 수학적 정리

- draw call 수: **6**
- 고유 texture 수: **7** (container 1 + side 6)
- 활성 (sampler, texture) 매핑 수: **6 × 2 = 12**

**공식**: `activations = draws × samplers_per_draw`. "draws = textures" 는 잘못된 등식.

### 게임 엔진 표준 패턴: bind 빈도로 unit 분리

상용 엔진은 texture unit 을 **"얼마나 자주 바뀌는지"** 로 구분해서 할당한다:

| Unit | 용도 | bind 빈도 | 언제 bind |
|------|------|----------|----------|
| 0 | albedo/diffuse | 모델마다 | 매 draw |
| 1 | normal map | 모델마다 | 매 draw |
| 2 | lightmap | 씬 시작 시 1회 | 한 번만 |
| 3 | shadow map | 프레임마다 1회 | 프레임 시작 |
| 4 | env cubemap | 거의 영구 | 초기화 시 |

**핵심 원칙**:
- 자주 바뀌는 unit 만 **매 draw** 에서 rebind
- 공유 자원은 **한 번만** bind (프레임 시작 / 씬 로딩 시)
- Draw call 별 GL 호출 수가 극적으로 감소

### ⚠️ 함정: Bind ≠ Render

**"CPU 에서 열심히 bind 했는데 화면에 안 반영됨"** 의 가장 흔한 원인:

```glsl
uniform sampler2D tex1;
uniform sampler2D tex2;   // ← 선언은 있지만...

void main() {
    vec4 c1 = texture(tex1, fs_in.vsTexCoord);
    // vec4 c2 = texture(tex2, fs_in.vsTexCoord);   ← 주석 처리된 상태
    fragColor = c1 * fs_in.vsColor;     // ← tex2 는 쓰이지 않음
}
```

이 상태에서 CPU 가 루프 안에서 `glBindTexture(..., side[f])` 를 아무리 호출해도, **FS 가 tex2 를 샘플링하지 않으므로 화면엔 영향 없음**. 모든 면이 container 로만 그려진다.

**판별법**: 루프에서 텍스처를 바꾸는데 면마다 같은 이미지로 보이면 → FS 가 해당 sampler 를 **실제로 사용** 하는지 확인.

### Material 설계에 주는 시사점

이 개념을 알면 Step 3 의 `Material` 설계가 자연스러워진다:

1. **Slot 은 sampler name 으로 식별** — unit 번호는 내부 디테일
2. **Slot 이 여러 개일 수 있음** — `tex1`, `tex2`, `normalMap`, ... 동시 보유 가능
3. **Apply() 는 각 slot 을 자기 unit 에 바인딩** — 한 번의 Apply 호출로 여러 텍스처 활성화
4. **Shared texture 는 Material 간 `shared_ptr<Texture>` 로 공유** — 같은 파일 참조 시 GPU 메모리 한 번만 점유

### 원칙 5가지 (외우기용)

1. **Texture unit = global persistent slot**. "해제" 가 아니라 "덮어쓰기" 로 관리.
2. **한 draw 는 여러 unit 을 동시 샘플링**. `sampler2D` uniform 개수만큼.
3. **Draw call 수 ≠ 텍스처 수**. 두 값은 독립 설계 변수.
4. **자주 바뀌는 것만 rebind, 공유 자원은 한 번만**. 엔진 성능 최적화의 기본.
5. **Bind ≠ Render**. FS 가 실제로 sampling 해야 화면 반영.

---

## 📚 보충 개념 2 — 규모가 커지면? (10 → 100 → 1,000,000)

> "시분할로 계속 bind 바꿔가면 무한히 그릴 수 있는 거 아냐?" 라는 자연스러운 의문의 답.

### 결론 먼저

**시분할 접근은 10~100 개 수준에서만 실용적**. 그 이상은 **집합화 + 가상화** 라는 완전히 다른 기법이 필요하다. 상용 엔진이 "수십만 개 텍스처" 를 다루는 것처럼 보이는 이유는 시분할이 아니라 이 기법들 덕분.

### 3단계 제약

#### ① Per-draw unit 상한 (하드웨어)
```cpp
GLint maxUnits;
glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits);
// 16~192 (플랫폼/세대에 따라)
```

한 draw call 에서 **동시에 바인딩 가능한 서로 다른 텍스처** 의 상한. 대부분 48~96.

#### ② Draw call overhead
```
60 FPS = 16.67 ms / frame
draw 1회 ≈ 5~50 μs
→ 프레임당 감당 가능한 draw: 수천 ~ 수만 개
→ 100만 draw 는 프레임이 수십 초 걸려서 애초에 불가
```

#### ③ VRAM 용량
```
100만 개 × 64×64 RGBA ≈ 21 GB  (최신 고급 GPU 가까스로)
100만 개 × 512×512    ≈ 1.3 TB (물리적 불가)
```

### 규모별 실전 해법

| 규모 | 기법 | 핵심 원리 |
|------|------|----------|
| **~10개** | 그냥 unit 바인딩 | 하드웨어 상한 (16) 이내 |
| **~100개** | **Texture Array** (`GL_TEXTURE_2D_ARRAY`) | 1 객체에 N 레이어, 1 bind 로 모두 접근 |
| **~수백 (2D/UI)** | **Texture Atlas** | 큰 이미지 하나에 여러 타일, UV 로 선택 |
| **수천~수만** | **Bindless Textures** (`ARB_bindless_texture`) | 텍스처 = 64-bit handle, 배열 인덱싱 |
| **수십만 이상** | **Sparse / Virtual Texturing** | logical texture, 페이지 단위 상주 |
| **월드 전체** | **LOD + Streaming** | 거리별 해상도 동적 조정 |

### 실전 셰이더가 픽셀당 실제로 읽는 텍스처 수

PBR 표준 머티리얼 하나:
- Albedo / Normal / Roughness / Metallic / AO / Emissive = **6**
- + Lightmap / Shadow / Env cube = **+3**
- + Detail / Mask = **+2~5**
- **합계 10~15 개** 정도

이게 셰이더 한 invocation 이 실제로 샘플링하는 숫자. 192 unit 상한이 훨씬 넉넉한 이유가 여기 있어 — 픽셀 하나를 그리는 데 필요한 텍스처는 생각만큼 많지 않아.

### 질문을 세분화하기

"몇 개를 그릴 수 있나" 는 **질문이 모호한 상태**. 정확히 답하려면:

| 질문 | 답 | 결정 요인 |
|------|-----|----------|
| 한 shader invocation 당 동시 샘플링 | **10~192** | 하드웨어 상한 |
| 한 frame 에 등장하는 고유 텍스처 | **수천~수만** | VRAM + draw call 예산 |
| 월드 어딘가에 존재하는 전체 | **수십만~백만** | 디스크 용량 |
| VRAM 에 상시 상주 | **수천~수만** | GPU 메모리 용량 |

### Material 설계에 주는 함의

exercise6 같은 학습 프로젝트는 **"~10 개 수준의 시분할 bind"** 스케일이면 충분. 이 단계에서 Material + Slot 추상화로 배워야 할 것:

1. **Slot 은 역할 기반으로 명명** (`albedo`, `normal`, `shadow`) — 나중에 PBR 로 확장 시 자연스러움
2. **Shared 자원 (lightmap, cubemap) 과 per-model 자원 분리** — 엔진 성능 최적화의 기초
3. **같은 텍스처 공유를 `shared_ptr<Texture>` 로** — VRAM 절약의 첫걸음

이게 Step 3 (Material) 과 Step 4 (범용 Draw) 가 궁극적으로 지향하는 구조야. 지금 당장은 "10 개 미만의 시분할" 이지만, **설계가 미리 확장 가능하게 잡혀 있으면** 나중에 Texture Array 나 Bindless 로 넘어갈 때 리팩토링이 최소화돼.

---

## 🪜 Step 3 — `Material` + `TextureSlot` 추상화

### 목표

"이 모델의 `tex1` sampler 에 이 텍스처를 바인딩하고 싶다" 를 **이름 기반으로 명시**한다. 인덱스 의존 (`mTextureAddrs[0]`) 제거.

### 왜 필요한가

현재의 `mTextureAddrs[0] = tex1, [1+f] = tex2` 관습은 **호출자의 약속**에 의존한다. 한 호출이라도 누락되면 `operator[]` OOB → UB. sbox 의 `TextureSlot { samplerName, texture, unit }` 은 이 관습을 **데이터 구조로 이동** 시킨다.

### 설계 초안

```cpp
struct TextureSlot
{
    std::string samplerName;             // "tex1", "tex2", "normalMap"
    std::shared_ptr<Texture> texture;    // Step 1 에서 만든 Texture
    int unit;                             // GL_TEXTURE0 + unit
};

class Material
{
  private:
    std::vector<TextureSlot> mSlots;
    vec4 mBaseColor = vec4(1, 1, 1, 1);
    vec2 mUVOffset  = vec2(0, 0);
    vec2 mUVRatio   = vec2(1, 1);

  public:
    // 🔗 Sampler 이름으로 텍스처 바인딩 — 역할 기반
    //    같은 samplerName 이 이미 있으면 교체, 없으면 추가
    //    반환값은 *this 로 체이닝 가능 (builder 패턴)
    Material& SetTexture(const std::string& samplerName,
                          std::shared_ptr<Texture> texture,
                          int unit = 0);

    // Uniform 상태 설정 (chaining)
    Material& SetBaseColor(vec4 c)  { mBaseColor = c; return *this; }
    Material& SetUVOffset(vec2 v)   { mUVOffset = v; return *this; }
    Material& SetUVRatio(vec2 v)    { mUVRatio = v; return *this; }

    // 🎨 glDrawElements 직전 호출 — 모든 state 를 한번에 주입
    void Apply(GLuint progAddr) const;
};
```

### `Apply` 구현 순서 (중요!)

```cpp
void Material::Apply(GLuint progAddr) const
{
    // 1️⃣ 스칼라 uniform — 타입 매칭 주의!
    //     (STUDY_NOTE Priority 1-6 — glUniform* 타입 불일치 교훈)
    glUniform4fv(glGetUniformLocation(progAddr, "baseColor"),     1, mBaseColor);
    glUniform2fv(glGetUniformLocation(progAddr, UNIFORM_UV_OFFSET), 1, mUVOffset);
    glUniform2fv(glGetUniformLocation(progAddr, UNIFORM_UV_RATIO),  1, mUVRatio);

    // 2️⃣ 각 슬롯: 유효한 텍스처만 바인딩
    for (const auto& slot : mSlots)
    {
        if (!slot.texture || !slot.texture->IsValid())
            continue;   // 🛡️ null/invalid 슬롯 방어 (OOB 버그 원천 차단)

        // (a) 해당 unit 활성화
        glActiveTexture(GL_TEXTURE0 + slot.unit);
        // (b) texture 바인딩
        glBindTexture(GL_TEXTURE_2D, slot.texture->GetAddr());
        // (c) sampler uniform 에 unit 번호 주입 (int — glUniform1i!)
        glUniform1i(glGetUniformLocation(progAddr, slot.samplerName.c_str()),
                    slot.unit);
    }
}
```

### 구현 체크리스트

- [ ] `SetTexture` 가 **같은 이름 있으면 교체, 없으면 추가** 동작하는지 (for 루프로 검색 후 `push_back`)
- [ ] `Apply()` 안의 모든 `glUniform*` 호출이 쉐이더 타입과 매칭되는지 재검증
  - `vec4` → `glUniform4fv`
  - `vec2` → `glUniform2fv`
  - `sampler2D` → `glUniform1i`
- [ ] `slot.texture` 가 `nullptr` 이거나 `!IsValid()` 일 때 **건너뛰기** (throw 하지 말 것 — 부분 실패 허용)
- [ ] Material 의 복사 의미론: `Texture` 는 `shared_ptr` 이라 복사 가능. Material 은 복사해도 OK (같은 텍스처를 공유).
- [ ] 체이닝 반환값 `*this` 로 `.SetTexture(...).SetUVRatio(...)` 사용 가능하게

### 검증

```cpp
Material m;
m.SetTexture("tex1", TextureCache::Load("./textures/container.jpg"), 0)
 .SetTexture("tex2", TextureCache::Load("./textures/side1.jpg"), 1)
 .SetUVRatio(vec2(1, 1));

// Draw 시 Apply 호출:
glUseProgram(progAddr);
m.Apply(progAddr);
glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
```

### ⚠️ 함정

- **`Apply()` 가 `const` 인 이유**: Material 의 상태는 바뀌지 않고 GL state 만 변경. 이걸 `const` 로 막아두면 "draw 중에 material 내용이 바뀌지 않는다" 는 계약을 컴파일러가 강제함.
- **`glUniform1i` 에 float 넘기지 말 것**. sampler uniform 은 정수. `slot.unit` 은 int 로 유지.
- **체이닝 반환은 참조** (`Material&`), 값 아님. 값으로 반환하면 복사본이 만들어져 원본 수정이 안 됨.

---

## 🪜 Step 4 — `ModelBase::Draw` 범용화

### 목표

`Draw()` 에서 **모든 큐브 전용 하드코딩을 제거** 하고, 임의의 메쉬를 올바르게 그릴 수 있도록 한다. Material 은 `Apply()` 가 알아서 처리.

### 왜 필요한가

현재 `Draw()` 에 박힌 `6`, `f*6`, `mTextureAddrs[1+f]` 는 **"이 모델은 큐브이다" 라는 전제**. Disk 를 넣으면 부채꼴이 되고, 텍스처 슬롯이 부족하면 OOB UB. Material 이 슬롯 관리를 가져가면 이 모든 게 사라진다.

### Before / After 비교

**Before (현재, 큐브 하드코딩)**:
```cpp
void Draw(GLuint prog_addr)
{
    glBindVertexArray(mVAOAddr);
    glUniformMatrix4fv(..., UNIFORM_MODEL_MAT, ..., GetModelMatrix());
    glUniformMatrix4fv(..., UNIFORM_UV_OFFSET, ..., mUVOffset);  // ❌ vec2 에 Matrix4fv
    glUniform1i(..., SAMPLER_TEX1, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);  // ❌ OOB 가능

    for (int f = 0; f < 6; f++)                       // ❌ 큐브 가정
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);  // ❌ OOB 가능
        glDrawElements(GL_TRIANGLES, 6, ..., (void*)(f * 6 * sizeof(GLuint))); // ❌ 6 하드코딩
    }
}
```

**After (범용)**:
```cpp
void Draw(GLuint prog_addr)
{
    glBindVertexArray(mVAOAddr);
    glUniformMatrix4fv(glGetUniformLocation(prog_addr, UNIFORM_MODEL_MAT),
                       1, false, GetModelMatrix());

    mMaterial.Apply(prog_addr);  // ✅ Material 이 uniform/sampler 전부 담당

    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
    //                           ^^^^^^^^^^^^ 메쉬 자신의 인덱스 수
}
```

### 변경할 것들

- [ ] `ModelBase` 멤버에 `Material mMaterial;` 추가 (public 접근자 `Material& GetMaterial()`)
- [ ] `mTextureAddrs`, `mUVOffset`, `mUVRatio` 멤버 **제거** — Material 로 이관
- [ ] `AddTexture(const char*)` 메서드 **제거** (또는 deprecated 로 표시) — `GetMaterial().SetTexture(...)` 로 대체
- [ ] `Draw()` 본문을 위 After 블록으로 교체
- [ ] `startup()` 의 모델 생성 코드를 새 API 로 전환:
  ```cpp
  auto model = std::make_unique<ModelBase>();
  model->Build(vertices);

  model->GetMaterial()
      .SetTexture("tex1", TextureCache::Load(TEXTURE_CONTAINER), 0)
      .SetUVRatio(vec2(1, 1));

  models.push_back(std::move(model));
  ```

### 검증

- [ ] Disk 모델: 부채꼴이 아닌 **전체 원** 이 그려지는지
- [ ] 큐브 모델: 텍스처가 정상 매핑되는지 (tex1 하나만 쓴다면 단일 텍스처로 덮여야 함)
- [ ] 텍스처 없는 모델 (AddTexture 호출 생략): **크래시 없이 렌더** 되는지 (Material 의 방어 코드가 작동)
- [ ] UV 애니메이션: `model->GetMaterial().SetUVOffset(vec2(t, 0))` 로 매 프레임 바꿨을 때 **실제로 움직이는지** (Priority 1-6 의 glUniform 타입 버그 재발 없는지)

### ⚠️ 함정

- **"큐브의 면별 다른 텍스처" 는 이 구조로 직접 해결되지 않는다.** Material 한 개 = draw call 한 번 에서 상태가 고정이므로. 면별 텍스처가 필요하면:
  - (a) **Sub-mesh** 구조로 확장: `std::vector<SubMesh>` 각자 `(indexStart, indexCount, material)`
  - (b) **Model 을 6개로 분리**: 각 model 이 면 하나를 담당 (Chapter7 패턴)
  - Step 4 자체는 (a), (b) 를 다루지 않음. "범용 Draw" 만 확보하고, 면별 기능은 별도 Step 으로 분리할 것.
- **기존 cube startup 코드가 깨진다면**, 일단 "Material 한 개만 가진 큐브 (모든 면 같은 텍스처)" 로 마이그레이션한 뒤, 면별 텍스처는 sub-mesh 방식으로 별도 구현.

---

## 🪜 Step 5 — Hot Reload (선택)

### 목표

파일 시스템에서 텍스처 파일이 수정되면 **재시작 없이** 게임 내 텍스처가 갱신되게 한다. sbox 의 `Hotload` 패턴.

### 왜 이게 뒤로 밀리나

파일 감시 (filesystem watcher) 인프라가 필요하고, Step 1~4 가 없으면 "뒤를 갈아끼울" 단단한 그릇이 없다. Texture wrapper 가 있어야 wrapper 는 유지하면서 내부 GL handle 만 교체할 수 있다.

### 설계 아이디어

```cpp
class Texture
{
public:
    // ...기존 멤버...

    // 🔁 현재 wrapper 를 유지한 채 내부 handle 만 새 파일 내용으로 교체
    bool Reload();

private:
    // Reload 가 이용: 기존 mAddr 는 glDelete, 새로 LoadFromFile 한 뒤 mAddr 갱신
};

class TextureCache
{
public:
    // 🔁 경로로 찾은 Texture 가 있다면 Reload 호출
    static void Hotload(const std::string& path);

    // File watcher 가 주기적으로 호출하거나, 사용자가 수동 트리거
};
```

### 구현 포인트

- `Reload` 는 **기존 wrapper 를 파괴하지 않음** — 쓰는 쪽에 있는 `shared_ptr<Texture>` 가 그대로 유효
- 다만 **GL handle 은 바뀔 수 있음** — 이 handle 을 외부에서 별도로 저장하고 있다면 stale. 외부 저장 금지 규칙을 문서화
- 파일 감시는 `inotify` (Linux), `FSEvents` (macOS), `ReadDirectoryChangesW` (Windows) 플랫폼 별 분기. **우선 수동 트리거부터** 만들고, 파일 감시는 나중에.

**학습 목표가 아니면 이 Step 은 건너뛰고 Step 7 으로 점프해도 OK.**

---

## 🪜 Step 6 — 비동기 로딩 (선택)

sbox 의 `LoadFromFileSystemAsync` 패턴. 웹에서 텍스처를 받거나, 로딩 중 프레임 hitch 를 피하고 싶을 때.

### 핵심 패턴

```cpp
// 1. Future/Promise 기반 반환
std::future<std::shared_ptr<Texture>> TextureCache::LoadAsync(const std::string& path);

// 2. 스레드에서 stbi_load (이미지 디코딩) 를 수행
// 3. 메인 스레드에서 GL 호출 (glTexImage2D) — GL 은 스레드 안전 아님
// 4. Completion 은 "다음 프레임에 flush" 형태로 메인 스레드 동기화
```

### 함정

- **GL 호출은 대부분 메인 스레드에서만 가능**. 이미지 디코딩은 멀티스레드 가능하지만, **GL 업로드는 메인 스레드로 돌려보내야** 함. sbox 가 `ThreadSafe.AssertIsMainThread()` 를 거는 이유가 이것.
- Future 대기 중 엔진이 종료될 수 있음 — cancellation token 패턴 필요.

**학습 프로젝트에서는 오버킬.** 건너뛰어도 무방.

---

## 🪜 Step 7 — `GetPixels<T>` (선택)

### 목표

디버깅/툴링용으로 GPU 텍스처를 CPU 로 읽어오는 제네릭 API. sbox 의 `GetPixels<T>` 패턴.

### 설계 초안

```cpp
template<typename T>
void Texture::GetPixels(int srcX, int srcY, int width, int height,
                        std::span<T> dstData, int mip = 0) const
{
    static_assert(std::is_trivially_copyable_v<T>, "T must be POD");

    // 🛡️ 방어 검증 (sbox 의 철학 ⑤)
    if (srcX < 0 || srcY < 0 || width <= 0 || height <= 0)
        throw std::invalid_argument("srcRect invalid");
    if ((int64_t)srcX + width  > mWidth ||
        (int64_t)srcY + height > mHeight)
        throw std::invalid_argument("srcRect out of range");
    if (dstData.size() * sizeof(T) < (size_t)width * height * sizeof(T))
        throw std::invalid_argument("dstData too small");

    // 실제 읽기
    glBindTexture(GL_TEXTURE_2D, mAddr);
    glGetTexImage(GL_TEXTURE_2D, mip, GL_RGBA, GL_UNSIGNED_BYTE, dstData.data());
}
```

### 언제 유용한가

- Color picker 구현
- 섬네일 생성
- 텍스처 해시 (중복 판별)
- 테스트 코드 (렌더 결과 검증)

당장 쓰임새가 없으면 구현 후 방치될 수 있음. 필요해지면 그때 만드는 게 합리적.

---

## 🎯 최종 검증 체크리스트

모든 Step 적용 후 아래가 전부 참이어야 한다:

### 기능
- [ ] Disk 모델이 **부채꼴 없이** 전체 원으로 렌더
- [ ] Cube 모델이 **텍스처 매핑** 된 상태로 렌더
- [ ] 텍스처 없는 모델도 **크래시 없이** 렌더 (null slot 방어)
- [ ] UV 애니메이션 (`SetUVOffset`) 이 **매 프레임 반영**
- [ ] 같은 파일을 여러 모델이 써도 **VRAM 에 한 번만** 올라감 (use_count 확인)

### 코드 품질
- [ ] `ModelBase` 에 `mTextureAddrs` / `mUVOffset` / `mUVRatio` / `mMaterial` 중 **`mMaterial` 만** 남음
- [ ] `glUniform*` 호출이 전부 **`Material::Apply`** 한 곳에 집중됨
- [ ] `operator[]` 로 vector 접근하는 곳이 **없음** (or 접근 전 size 체크)
- [ ] `Draw()` 에 특정 메쉬 상수 (`6`, `f*6`) 하드코딩 **없음**
- [ ] 텍스처 로드 실패 시 `std::cerr` 로 경고 출력

### 재발 방지 (STUDY_NOTE 교훈)
- [ ] R-1: `mScale` 기본값 `(1,1,1)` 유지
- [ ] R-2: EBO `glBufferData` 가 `mElementData` 사용
- [ ] R-3: attribute offset 누적 계산
- [ ] R-4: vector `operator[]` 가드 (Material slot 루프의 nullptr 체크)
- [ ] 1-6: `glUniform*` 타입 매칭 (`vec2` → `2fv`, `mat4` → `Matrix4fv`, `sampler` → `1i`)

---

## 📚 학습 포인트 요약

이 로드맵을 완주하면 다음 개념이 체득된다:

1. **RAII 기반 GL 리소스 관리** — 생성자/소멸자 + move-only + `shared_ptr`
2. **Factory + Cache 패턴** — 단일 진입점 + identity 기반 중복 제거
3. **Strategy 인터페이스** — Sampler 이름으로 추상화된 바인딩
4. **관심사 분리 (SoC)** — Geometry (ModelBase), State (Material), Resource (Texture), Registry (TextureCache) 각자의 책임
5. **방어적 프로그래밍** — 모든 경계에서 검증, 실패를 명시적으로 처리
6. **타입 안전성** — `glUniform*` 타입 매칭, `where T : unmanaged` 같은 제약

이건 "코드가 잘 돌아가는 것" 을 넘어서 **"구조적으로 버그가 발생할 수 없는 상태"** 를 설계하는 훈련이다. sbox 같은 상용 엔진이 수백만 라인 코드베이스를 유지할 수 있는 이유가 바로 이런 구조적 방어선 덕분.

---

## 📎 참고

- **sbox 원본**: `Sandbox.Engine/Resources/Textures/Texture.Load.cs`, `Texture.Read.cs`
- **이전 교훈**: [STUDY_NOTE.md](./STUDY_NOTE.md) — Exercise6 챕터의 R-1~R-4, Priority 1-6
- **현재 코드**: [apps/exercise6/main.cpp](../apps/exercise6/main.cpp)

---

## 💡 진행 팁

- **한 Step 씩** 완료하고 빌드/실행까지 확인한 뒤 다음 Step 으로. 여러 Step 을 한꺼번에 건드리면 문제 원인 파악이 어려워짐.
- 각 Step 완료마다 **git commit** — "Step 1: Texture class separation" 같은 메시지로. 되돌릴 수 있는 상태를 유지.
- 막히면 **체크리스트를 다시 읽고**, 그래도 안 되면 STUDY_NOTE 의 해당 Priority 항목을 참고.
- 구현 중 발견한 새 실수는 **STUDY_NOTE 에 추가** — 이 프로젝트는 "노트에 쌓이는 교훈" 자체가 학습 산출물.
