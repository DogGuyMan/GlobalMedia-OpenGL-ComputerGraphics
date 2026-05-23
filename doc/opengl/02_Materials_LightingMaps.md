## Materials & Lighting Maps — Material 구조체와 텍스처 유닛 분리

> 출처 노트: `멀티플라이팅.md` §3
> LearnOpenGL 매핑: Lighting — 3. Materials / 4. Lighting maps

---

> ### 📄 1. `Material` 구조체 확장

단일 광원 베이스(`exercise8_win`)에서 다중 광원 버전(`chapter8`)으로 가며 `Material` 에 **텍스처 이미지 유닛 번호** 와 **shininess** 가 추가됐다.

| 필드 | 단일 광원 | 다중 광원 |
|---|---|---|
| `diffuseTexture` (GL 핸들) | ✅ | ✅ |
| `specularTexture` (GL 핸들) | ✅ | ✅ |
| `diffuseUnit` (이미지 유닛 번호) | ❌ | ✅ (`1` = `GL_TEXTURE1`) |
| `specularUnit` | ❌ | ✅ (`2` = `GL_TEXTURE2`) |
| `shininess` | ❌ | ✅ (`32.0f`) |

핵심은 **GL 핸들과 유닛 번호를 둘 다 한 구조체에 들고 있다**는 점이다.

---

> ### 📄 2. "핸들 vs 유닛 번호" 분리를 헬퍼로 묶기

`sampler2D` 유니폼에는 **텍스처 객체 핸들이 아니라 텍스처 이미지 유닛 번호**를 넣어야 한다. (왜 그런가의 메커니즘은 `01_GettingStarted/03_Textures_샘플러유닛.md` 참조 — 그쪽이 단일 출처.)

단일 광원 버전은 `glUniform1i(..., 1)` / `glActiveTexture(GL_TEXTURE1)` 를 손으로 맞췄지만, 다중 광원 버전은 이 한 쌍을 헬퍼로 묶었다:

- `UniformsSetMaterial(program, "material", mat)` → `material.diffuse` 에 `mat.diffuseUnit`, `material.specular` 에 `mat.specularUnit`, `material.shininess` 전달.
- `BindMaterialTextures(mat)` → `glActiveTexture(GL_TEXTURE0 + mat.diffuseUnit); glBindTexture(...)` — 셰이더에 알려준 것과 **같은 유닛**에 실제 텍스처를 바인딩.

→ "셰이더에 번호를 알려주는 쪽"과 "그 번호에 텍스처를 거는 쪽"이 항상 한 데이터(`Material`)에서 나오므로 어긋날 일이 없다.

---

> ### 📄 3. 셰이더 측 Material struct

```glsl
struct Material {
    sampler2D diffuse;   // 유닛 번호를 받음 (예: 1)
    sampler2D specular;  // 유닛 번호를 받음 (예: 2)
    float     shininess; // specular pow 지수
};
uniform Material material;
```

shininess 는 Phong specular 의 `pow(max(dot(...), 0.0), shininess)` 지수로 전달되어 하이라이트의 뾰족함을 결정한다.

## 시험 포인트 요약
- 샘플러 uniform = **유닛 인덱스**, 텍스처 핸들 아님. (메커니즘: Textures 노트)
- `Material` 이 핸들·유닛·shininess 를 함께 들고, "유니폼 전달"과 "텍스처 바인딩"이 같은 데이터에서 파생되게 하면 유닛/핸들 어긋남이 구조적으로 차단됨.

## 관련 노트
- 핸들 vs 유닛 번호 메커니즘·함정·배치 (SSoT): `01_GettingStarted/03_Textures_샘플러유닛.md`
- 이 Material 을 3종 광원과 합산하는 방식: `02_Lighting/04_MultipleLights.md`
