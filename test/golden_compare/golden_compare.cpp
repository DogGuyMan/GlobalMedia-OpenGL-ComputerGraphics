// =============================================================================
// golden_compare.cpp -- 골든 이미지 비교 게이트 (순수 CPU, GL 불필요)
// =============================================================================
//
// _MyApp_ capture 모드(env SJH_GOLDEN_CAPTURE=1)가 생성한 PNG 3장을
// repo 에 커밋된 골든과 OpenCV 로 비교한다. GL/SJH 모듈 미링크 -- OpenCV
// (imgcodecs) 로 PNG 로드/쓰기 + 픽셀 diff 만 수행.
//
// 판정: 차이픽셀 / 전체픽셀 > maxPixelFraction(=0.05) 이면 FAIL.
//       FAIL 시 diff PNG 를 <build>/.../artifacts/diff_<name>.png 로 기록.
//
// diff 산출: cv::absdiff 로 채널별 절대차 -> 픽셀당 최대 채널차 reduce ->
//            (최대 채널차 > kChannelDiffThreshold) 인 픽셀 수를 countNonZero 로 셈.
//            noise floor=0(비트동일 캡처) 이라 임계는 0(완전일치) 로 둔다.
//
// 경로 주입(CMake compile-def, 절대경로):
//   SJH_GOLDEN_CAPTURED_DIR = 캡처 산출 dir (build/apps/_MyApp_/test/golden)
//   SJH_GOLDEN_REF_DIR      = 커밋 골든 dir (repo test/golden)
//   SJH_GOLDEN_ARTIFACT_DIR = diff 아티팩트 출력 dir

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

// === 경로 주입 (CMake compile-def, 미정의 시 컴파일 에러로 즉시 노출) ===========
#ifndef SJH_GOLDEN_CAPTURED_DIR
#error "SJH_GOLDEN_CAPTURED_DIR 미정의 -- CMake compile-def 로 주입해야 함"
#endif
#ifndef SJH_GOLDEN_REF_DIR
#error "SJH_GOLDEN_REF_DIR 미정의 -- CMake compile-def 로 주입해야 함"
#endif
#ifndef SJH_GOLDEN_ARTIFACT_DIR
#error "SJH_GOLDEN_ARTIFACT_DIR 미정의 -- CMake compile-def 로 주입해야 함"
#endif

// 차이픽셀 비율 예산. ★ 비교 FAIL 시 이 값을 절대 올리지 말 것(게이트 무력화).
constexpr double kMaxPixelFraction = 0.05;

// 픽셀당 "차이" 판정 임계(채널 절대차). noise floor=0 이라 0 초과면 곧 차이.
constexpr int kChannelDiffThreshold = 0;

/// 비교 1건 결과.
struct CompareResult
{
	bool        loadedCaptured = false;
	bool        loadedRef      = false;
	bool        sizeMatch      = false;
	int         capturedW = 0, capturedH = 0;
	int         refW = 0, refH = 0;
	long long   diffPixels  = -1;  //!< 차이픽셀 수 (-1 = precondition 실패).
	long long   totalPixels = 0;
	double      diffFraction = 0.0;
	bool        passed       = false;
	std::string diffArtifactPath; //!< FAIL 시 기록한 diff PNG 경로(없으면 빈 문자열).
};

/// 캡처 PNG vs 골든 PNG 비교. name = "golden_full" 등(확장자 제외).
CompareResult Compare(const std::string &name)
{
	CompareResult r;

	const std::string capturedPath = std::string(SJH_GOLDEN_CAPTURED_DIR) + "/" + name + ".png";
	const std::string refPath       = std::string(SJH_GOLDEN_REF_DIR) + "/" + name + ".png";

	// IMREAD_UNCHANGED: 알파 포함 원본 채널 그대로 로드(캡처/골든 동일 포맷 가정).
	const cv::Mat cap = cv::imread(capturedPath, cv::IMREAD_UNCHANGED);
	const cv::Mat ref = cv::imread(refPath, cv::IMREAD_UNCHANGED);

	r.loadedCaptured = !cap.empty();
	r.loadedRef      = !ref.empty();
	if (!r.loadedCaptured || !r.loadedRef)
	{
		return r; // 로드 실패 -> 즉시 fail (passed=false).
	}

	r.capturedW = cap.cols;
	r.capturedH = cap.rows;
	r.refW      = ref.cols;
	r.refH      = ref.rows;
	// 크기 + 채널수 + 타입 모두 동일해야 absdiff 가능. 하나라도 어긋나면 즉시 fail.
	r.sizeMatch = (cap.cols == ref.cols && cap.rows == ref.rows && cap.type() == ref.type());
	if (!r.sizeMatch)
	{
		return r; // 크기/포맷 불일치 -> 즉시 fail.
	}

	// 채널별 절대차 -> 채널 축으로 최대값 reduce -> 픽셀당 최대 채널차(단일 채널).
	cv::Mat diff;
	cv::absdiff(cap, ref, diff);

	cv::Mat perPixelMax;
	const int channels = diff.channels();
	if (channels > 1)
	{
		// (H*W, C) 로 reshape 후 행(=픽셀) 단위 가로 reduce-MAX -> (H*W, 1).
		const cv::Mat flat = diff.reshape(1, diff.rows * diff.cols); // CV_8U, cols=channels
		cv::reduce(flat, perPixelMax, 1 /*dim=각 행 축소*/, cv::REDUCE_MAX, CV_8U);
		perPixelMax = perPixelMax.reshape(1, diff.rows); // 다시 (H, W) 단일 채널.
	}
	else
	{
		perPixelMax = diff;
	}

	// (최대 채널차 > 임계) 인 픽셀 마스크 -> 차이픽셀 수.
	cv::Mat mask = perPixelMax > kChannelDiffThreshold; // CV_8U (0/255).
	const long long diffPixels = static_cast<long long>(cv::countNonZero(mask));

	r.diffPixels   = diffPixels;
	r.totalPixels  = static_cast<long long>(cap.cols) * cap.rows;
	r.diffFraction = (r.totalPixels == 0)
	                     ? 1.0
	                     : static_cast<double>(diffPixels) / static_cast<double>(r.totalPixels);
	r.passed       = (r.diffFraction <= kMaxPixelFraction);

	// FAIL 시에만 diff 아티팩트 기록(통과 시 디스크 낭비 방지).
	if (!r.passed)
	{
		std::error_code ec;
		fs::create_directories(SJH_GOLDEN_ARTIFACT_DIR, ec);
		const std::string out = std::string(SJH_GOLDEN_ARTIFACT_DIR) + "/diff_" + name + ".png";
		// 차이 강조: 절대차 diff 자체를 기록(밝을수록 차이 큼). 알파가 있으면 떼고 BGR 로.
		cv::Mat vis = diff;
		if (vis.channels() == 4)
		{
			std::vector<cv::Mat> ch;
			cv::split(diff, ch);
			ch.resize(3); // BGR 만(알파 drop).
			cv::merge(ch, vis);
		}
		if (cv::imwrite(out, vis))
		{
			r.diffArtifactPath = out;
		}
	}

	return r;
}

/// 실패 시 사람이 읽을 진단 메시지.
std::string Describe(const std::string &name, const CompareResult &r)
{
	std::ostringstream os;
	os << "[golden] " << name << "\n";
	os << "  captured = " << std::string(SJH_GOLDEN_CAPTURED_DIR) << "/" << name << ".png\n";
	os << "  ref      = " << std::string(SJH_GOLDEN_REF_DIR) << "/" << name << ".png\n";
	if (!r.loadedCaptured)
	{
		os << "  FAIL: 캡처 PNG 로드 실패(capture 미실행/경로 오류?)";
		return os.str();
	}
	if (!r.loadedRef)
	{
		os << "  FAIL: 골든 PNG 로드 실패";
		return os.str();
	}
	if (!r.sizeMatch)
	{
		os << "  FAIL: 크기 불일치 captured=" << r.capturedW << "x" << r.capturedH
		   << " ref=" << r.refW << "x" << r.refH;
		return os.str();
	}
	os << "  diffPixels=" << r.diffPixels << " / total=" << r.totalPixels
	   << " (fraction=" << r.diffFraction << ", budget=" << kMaxPixelFraction << ")";
	if (!r.passed)
	{
		os << "\n  FAIL: 차이픽셀 비율이 예산 초과";
		if (!r.diffArtifactPath.empty())
		{
			os << "\n  diff artifact = " << r.diffArtifactPath;
		}
	}
	return os.str();
}

// === MatchesGolden 매처 (catch2_pipeline §3 패턴) =============================
// SUT = 골든 이름(string). 매처가 캡처 vs 골든 비교를 수행하고 PASS/FAIL 판정.
struct MatchesGoldenMatcher : Catch::Matchers::MatcherGenericBase
{
	bool match(const std::string &name) const
	{
		mResult = Compare(name);
		mName   = name;
		return mResult.passed;
	}

	std::string describe() const override
	{
		return "은(는) 골든과 일치해야 함:\n" + Describe(mName, mResult);
	}

	mutable CompareResult mResult;
	mutable std::string   mName;
};

inline MatchesGoldenMatcher MatchesGolden()
{
	return MatchesGoldenMatcher{};
}

/// REF_DIR 의 golden_*.png 를 전수 수집(확장자 제외 stem). 정렬로 케이스 순서 결정적.
/// 새 골든 추가 = 비교코드 무변경(파일만 커밋하면 자동 포함).
std::vector<std::string> GoldenNames()
{
	std::vector<std::string> names;
	for (const auto &e : fs::directory_iterator(SJH_GOLDEN_REF_DIR))
	{
		const std::string stem = e.path().stem().string();
		if (e.path().extension() == ".png" && stem.rfind("golden_", 0) == 0)
			names.push_back(stem);
	}
	std::sort(names.begin(), names.end());
	return names;
}

} // namespace

// === glob 전수 비교 (데이터주도) ============================================
// REF_DIR 의 golden_*.png 를 모두 캡처본과 비교. 골든 추가 시 이 파일 무변경.

TEST_CASE("골든 전수 비교(glob)", "[golden]")
{
	const std::vector<std::string> names = GoldenNames();
	// ref dir 가 비면 게이트가 유명무실 - 최소 baseline 3장은 있어야 함.
	REQUIRE(names.size() >= 3);
	for (const auto &name : names)
	{
		DYNAMIC_SECTION("golden: " << name)
		{
			REQUIRE_THAT(name, MatchesGolden());
		}
	}
}
