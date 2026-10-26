#!/bin/bash
# 把本地仓库与 upstream 完全同步，并推送到自己的 fork(origin)。
# 用法: ./update_merge.sh [分支名，默认 master]
set -e

cd "$(dirname "$0")"

BRANCH=${1:-master}
UPSTREAM_REMOTE=upstream
ORIGIN_REMOTE=origin
BACKUP_BRANCH="backup/${BRANCH}-local"

git fetch "$UPSTREAM_REMOTE" --prune
git fetch "$ORIGIN_REMOTE" --prune

if [ -n "$(git status --porcelain)" ]; then
	echo "警告: 工作区有未提交改动，reset --hard 会丢弃它们。"
	read -r -p "确认继续? [y/N] " ans
	[ "$ans" = "y" ] || [ "$ans" = "Y" ] || exit 1
fi

# 保留目标分支当前状态到备份分支，便于回退: git reset --hard backup/<branch>-local
git branch -f "$BACKUP_BRANCH" "$BRANCH"

# 上游新增子模块时，本地若残留同名未跟踪目录会阻塞 checkout
# (untracked working tree files would be overwritten)，先清掉
git show "$UPSTREAM_REMOTE/$BRANCH:.gitmodules" 2>/dev/null | sed -n 's/^[[:space:]]*path = //p' | while read -r p; do
	if [ -d "$p" ] && [ -z "$(git ls-files "$p")" ]; then
		echo "清理残留目录: $p"
		rm -rf "$p"
	fi
done

git checkout "$BRANCH"
git reset --hard "$UPSTREAM_REMOTE/$BRANCH"

git submodule sync --recursive
git submodule update --init --recursive --force

git push --force-with-lease "$ORIGIN_REMOTE" "$BRANCH"

echo "同步完成: $(git rev-parse --short HEAD) $(git log -1 --format=%s)"

# 上游删除子模块后，本地会留下未跟踪目录(如 ffmpeg/)，提示手动处理
LEFTOVER=$(git status --porcelain | grep '^??' || true)
if [ -n "$LEFTOVER" ]; then
	echo "以下未跟踪内容需手动处理(上游已移除的子模块残留等):"
	echo "$LEFTOVER"
fi

echo "提示: 把上游更新合进开发分支 -> git checkout dev && git merge master"
