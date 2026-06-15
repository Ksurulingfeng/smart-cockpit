#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"          # 获取脚本所在目录的绝对路径
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"                        # 获取项目根目录
PROJECT_NAME=$(basename "$PROJECT_ROOT")                            # 获取项目名称（当前文件夹名）
REMOTE_SERVER="root@192.168.10.50"                                  # 远程服务器地址
REMOTE_DIR="~/$PROJECT_NAME"                                        # 远程目录
LOCAL_RESOURCES="$PROJECT_ROOT/src/resources"                       # 本地资源路径

echo "开始部署资源到 $REMOTE_SERVER ..."
ssh $REMOTE_SERVER "mkdir -p $REMOTE_DIR/media/music/"
ssh $REMOTE_SERVER "mkdir -p $REMOTE_DIR/media/videos/"
scp $LOCAL_RESOURCES/media/music/* $REMOTE_SERVER:$REMOTE_DIR/media/music/
scp $LOCAL_RESOURCES/media/videos/* $REMOTE_SERVER:$REMOTE_DIR/media/videos/