## Instancing — Per-Instance Attribute

> 출처 노트: `AttributeNBuffer.md` 시나리오 4
> LearnOpenGL 매핑: Advanced OpenGL — 10. Instancing

---

> ### 📄 고성능 instancing 의 per-instance VBO

"Square 1000개는 VAO/VBO/EBO 1개씩으로 충분" 이라는 직관은 **단순 draw 루프** 에는 맞다. 하지만 **고성능 instancing** 으로 그리려면 per-instance VBO 한 개가 *추가로* 필요하다.

```
VAO 1개
 ├── VBO #1 (per-vertex):  pos, normal, uv         — divisor 0 (vertex 마다 갱신)
 ├── VBO #2 (per-instance): modelMat, instanceColor — divisor 1 (instance 마다 갱신)
 └── EBO   (인덱스)
```

| 자원 | 개수 |
|---|---|
| VAO | 1 |
| VBO | **2** (per-vertex + per-instance) |
| EBO | 1 |

---

> ### 📄 핵심 메커니즘

- `glVertexAttribDivisor(attribIdx, 1)` 로 attribute 가 vertex-rate 가 아닌 *instance-rate* 로 동작
  - divisor 0 = 정점마다 갱신 (일반 attribute)
  - divisor 1 = 인스턴스마다 갱신 (per-instance)
- mesh 데이터(고정)와 인스턴스별 데이터(가변)는 update 빈도·접근 패턴이 다르므로 VBO 분리가 자연스럽다
- `glDrawElementsInstanced(..., 1000)` 한 번으로 1000개 큐브 → **CPU 호출 1회, GPU 가 N번 펼침**

> 단순 draw 루프는 CPU 가 draw call 을 N번 — instancing 은 1번. CPU↔GPU 통신 병목이 큰 대량 렌더에서 결정적 차이.

## 시험 포인트 요약
- 같은 mesh 를 위치만 바꿔 N개 → 그 자체로는 분할 불필요 (uniform 만 바꿔 draw N번).
- **고성능** instancing → per-instance VBO 추가 (VBO 2개).
- `glVertexAttribDivisor(idx, 1)` = instance-rate. `glDrawElementsInstanced` = 단일 호출 N 전개.

## 관련 노트
- 버퍼 분할 5가지 시나리오 전체 맥락: `01_GettingStarted/02_AdvancedData_버퍼분할.md`
