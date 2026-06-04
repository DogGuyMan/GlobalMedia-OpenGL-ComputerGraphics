import os
import sys
import subprocess

# 사용자가 제공한 해시 및 커밋 목록 (최신 -> 과거)
RAW_COMMITS = """
"""
# # 사용자가 제공한 해시 및 커밋 목록 (최신 -> 과거)
# RAW_COMMITS = """
# 53bc25e feat(vfx): main 로드 루프에 .efk 텍스처 검증 배선
# """

def get_target_hashes():
    # 문자열에서 해시값만 추출한 뒤, 과거 커밋부터 수정해야 하므로 역순 정렬합니다.
    lines = [line.strip() for line in RAW_COMMITS.strip().split('\n') if line.strip()]
    lines.reverse()
    return [line.split()[0] for line in lines]

def is_target_hash(commit_hash, target_hashes):
    # Git 내부 해시 길이 포맷과 유연하게 매칭되도록 처리
    for h in target_hashes:
        if h.startswith(commit_hash) or commit_hash.startswith(h):
            return True
    return False

def main():
    # ---------------------------------------------------------------------------------
    # (A) 내부적으로 Git 시퀀스 에디터(Rebase 자동화 조작기)로 스크립트가 호출된 경우
    # ---------------------------------------------------------------------------------
    if os.environ.get("AUTO_REWORD_EDITOR") == "1":
        todo_file = sys.argv[1]
        
        with open(todo_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
        with open(todo_file, 'w', encoding='utf-8') as f:
            for line in lines:
                parts = line.split()
                # 범위 내의 모든 'pick'을 'reword'(메시지 수정 모드)로 자동 변환
                if len(parts) >= 2 and parts[0] == 'pick':
                    f.write(line.replace('pick', 'reword', 1))
                    continue
                f.write(line)
        sys.exit(0)

    # ---------------------------------------------------------------------------------
    # (B) 사용자가 직접 파이썬 스크립트를 실행했을 때 동작 (메인 진입점)
    # ---------------------------------------------------------------------------------
    print("=== VSCode 커밋 메시지 순차 수정 파이썬 스크립트 (자동 해시판) ===")
    print("하드코딩된 해시 없이, 최근 N개의 커밋을 지정하여 순차적으로 메시지를 수정할 수 있습니다.\n")
    
    try:
        user_input = input("최근부터 몇 개의 커밋 메시지를 수정하시겠습니까? (이전 목록 기준 52 입력) : ")
        count = int(user_input.strip())
        if count <= 0:
            print("1 이상의 숫자를 입력해주세요.")
            return
    except ValueError:
        print("올바른 숫자를 입력해주세요.")
        return
        
    # 시작점: 현재 기준 지정된 숫자만큼 거슬러 올라감
    base_commit = f"HEAD~{count}"
    print(f"[실행] git rebase -i {base_commit}\n")
    
    # 환경변수를 조작하여 이 파이썬 스크립트를 Rebase 자동화기로, 
    # 실제 메시지 에디터를 VSCode(code --wait)로 강제 고정합니다.
    env = os.environ.copy()
    env["AUTO_REWORD_EDITOR"] = "1"
    env["GIT_SEQUENCE_EDITOR"] = f'"{sys.executable}" "{os.path.abspath(__file__)}"'
    env["GIT_EDITOR"] = "code --wait"
    
    # 해당 쉘 커맨드 실행: 파이썬이 모든 걸 통제하며 VSCode를 띄우기 시작합니다.
    result = subprocess.run(f"git rebase -i {base_commit}", shell=True, env=env)
    
    # 중복/빈 커밋으로 인해 Rebase가 멈췄을 때 자동으로 skip 하도록 처리
    while result.returncode != 0:
        try:
            git_dir = subprocess.check_output("git rev-parse --git-dir", shell=True, text=True, stderr=subprocess.DEVNULL).strip()
            is_rebasing = os.path.exists(os.path.join(git_dir, "rebase-merge")) or os.path.exists(os.path.join(git_dir, "rebase-apply"))
        except Exception:
            is_rebasing = False
            
        if is_rebasing:
            print("\n[자동화] 중복/빈 커밋으로 인한 충돌 감지! 'git rebase --skip'을 자동으로 실행합니다...")
            result = subprocess.run("git rebase --skip", shell=True, env=env)
        else:
            print("\n[!] Rebase가 알 수 없는 이유로 중단되었습니다.")
            break

    if result.returncode == 0:
        print("\n=== 모든 대상 커밋의 메시지 수정이 순차적으로 완료되었습니다! ===")

if __name__ == "__main__":
    main()
