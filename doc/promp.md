```
[ROLE]
[FIRST READ — 반드시 순서대로]
[CURRENT STATE]
[dev] : material eager delete`
[TASK — SP-UniversalRenderTarget]
[PHASE 분해 — 순서 준수]
[수용 기준 — 모든 항목 만족해야 commit]
[금지 사항 — 이전 거부 결정 함정]
[컨벤션 가드레일]
[체크인 시점 — 각 Phase 끝]
[진행 모드]
[질문해야 할 시점]
```

---

```
[ROLE]
[FIRST READ — 반드시 순서대로]
[CURRENT STATE]
[TASK — ImGui Layer 분리]
[PHASE 분해 — 플랜 Task 번호 그대로]
[수용 기준]
[금지 사항 — 이미 결정된 사항, 재논의 없이 따를 것]
[컨벤션 가드레일]
[체크인 시점]
[진행 모드]
[첫 행동]
```

----

```
[1]

Phase 2	LightUniformDispatcher 분리 (src/render/)	🔥
Phase 2.5	Light 추상 base + 가상 Apply(prog, slot)	🔥
Phase 3	SP-ProgramRegistry (CreateProgram/FindProgram)	🔥

진행해야 함.

---

[2]

그리고, 앞으로 모든 OpenGL 렌더링은  RenderTarget이 반드시 필요하며 카메라도 렌더 타겟 없이는 그려질 수 없도록
프로젝트를 리펙토링 할 것이다
즉 Camera는 반드시 RenderTarget이 필요하다는거. 그리고 그러한 렌더 타겟은 대표적으로 Screen을 덮는
Screen Rect Plane Mesh가 필요할 것으로 예상된다.


---
[3]

[2]번 과정은 Screen에 UI를 그리는 Renderer 확장 과정과도 연결된다
그렇게 함으로서 SP-RenderStage (IRenderStage 인터페이스)	🔥 높음	ImGui/PostFX/Skybox 통합 seam 과정도 수행 할 수 있게 되는것이다

---

[4]
의뢰를 정리하고 내가 요청한 내용에서 이해 안되거나 의사결정에 있어서 추가 정보가 필요하다면 질의해
그리고 너가 이해한 바와 내가 요청한 바가 맞는지 일체화 시켜보자

---

[5]
의도한 내용을 이해했다면 Unity, Unreal, Cocos2D, Godot 같은 모범 엔진의 API를 살펴보고 계획을 구체화 해야하니
Context7 을 사용해서 고품질 레퍼런스를 얻자

---

[1][2][3][4][5] 과정에서 하나하나 나에게 질의하면서 진행해
```

----

PostFXPass = MeshRenderer와 동일한 개념으로 의문 제기
AI는 3번이나 걸고 넘어졌지만
내가 그 근거를 반박하고 나니 둘다 동일하게 처리할 수 있다는 결론을 내림