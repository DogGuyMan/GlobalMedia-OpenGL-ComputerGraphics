/**
 * @file render_stage.cpp
 * @brief IRenderStage vtable 홈 TU.
 *
 * @details
 *  ### 책임
 *  - 헤더 전용 순수 추상 클래스 @c IRenderStage 의 vtable / typeinfo 를 *단일 번역 단위*에 정착.
 *
 *  ### 비-책임
 *  - [X] 런타임 로직 없음 - 내용 없음이 의도.
 *
 * @note header-only abstract 의 ODR 보장:
 *       다른 .cpp 들이 vtable 의 외부 정의를 이 TU 하나에서 받는다.
 *       본 파일에 구현 코드를 추가하지 말 것.
 */
#include "render/render_stage/render_stage.h"
