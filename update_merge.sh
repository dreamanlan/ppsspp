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

# 保留当前状态到备份分支，便于回退: git reset --hard backup/<branch>-local
git branch -f "$BACKUP_BRANCH" HEAD

git checkout "$BRANCH"
git reset --hard "$UPSTREAM_REMOTE/$BRANCH"

git submodule sync --recursive
git submodule update --init --recursive --force

git push --force-with-lease "$ORIGIN_REMOTE" "$BRANCH"

echo "同步完成: $(git rev-parse --short HEAD) $(git log -1 --format=%s)"
