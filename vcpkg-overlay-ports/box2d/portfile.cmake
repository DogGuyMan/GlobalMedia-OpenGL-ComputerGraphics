# box2d 2.4.1 overlay port — 공식 port 의 잘못된 테스트 비활성 옵션명 교정.
# 공식 port 는 -DBUILD_TESTS=OFF / -DBUILD_SAMPLES=OFF 를 넘기지만 box2d 2.4.1 의
# 실제 옵션명은 BOX2D_BUILD_UNIT_TESTS / BOX2D_BUILD_TESTBED 라서 무시됨 ->
# unit-test 가 빌드되며 번들 doctest.h 의 __asm__("int $3") (x86 breakpoint) 가
# arm64 에서 "unrecognized instruction mnemonic" 로 컴파일 실패.
vcpkg_fail_port_install(ON_TARGET "uwp")

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO erincatto/Box2D
    REF 9ebbbcd960ad424e03e5de6e66a40764c16f51bc  #v2.4.1
    SHA512 d9fa387ce893ed1fb73f80006491202f2624ef6d0fb37daf92fbd1a7f9071c84da45e4b418b333566435bbbdfd3d5f68a42dfca02416e9a3a2b4db039f1c6151
    HEAD_REF master
)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DBOX2D_BUILD_UNIT_TESTS=OFF
        -DBOX2D_BUILD_TESTBED=OFF
        -DBOX2D_BUILD_DOCS=OFF
)
vcpkg_install_cmake()

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/box2d)

vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
