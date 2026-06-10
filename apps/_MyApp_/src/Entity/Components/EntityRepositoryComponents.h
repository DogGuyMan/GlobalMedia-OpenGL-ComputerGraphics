/**
 * @file EntityRepositoryComponents.h
 * @brief 엔티티 내 형제 Component 를 키-값으로 등록/조회하는 Repository Component (stub - 미구현).
 *
 * @details
 *  ### 의도 (계획)
 *  - Actor::GetComponent<T>() 의 O(n) 선형 탐색을 보완하기 위해, 자주 참조되는 형제 Component 를
 *    미리 이름/타입으로 캐시해 두는 Repository 패턴 컴포넌트.
 *  - @c BaseEntity 는 현재 OnEnter 에서 직접 GetComponent 호출로 핫패스를 해소하고 있으므로
 *    본 컴포넌트 도입 전까지는 해당 패턴으로 대체 중.
 *
 *  ### 비-책임
 *  - [X] Component 생명주기 관리 - Actor 가 소유권 보유.
 *  - [X] 게임플레이 로직 - 순수 레지스트리/조회 역할.
 *
 * @note 현재 이 파일은 stub (빈 상태). 구현 추가 시 헤더 가드 + 네임스페이스
 *       @c TopdownShooter::Entity::Components 하에 선언할 것.
 */
