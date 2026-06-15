#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"          # 获取脚本所在目录的绝对路径
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"                        # 获取项目根目录
PROJECT_NAME=$(basename "$PROJECT_ROOT")                            # 获取项目名称（当前文件夹名）
REMOTE_SERVER="root@192.168.26.48"                                  # 远程服务器地址
REMOTE_DIR="~/$PROJECT_NAME"                                        # 远程目录
LOCAL_EXECUTABLE="$PROJECT_ROOT/build/arm-release/$PROJECT_NAME"    # 本地可执行文件路径
# LOCAL_EXECUTABLE="$PROJECT_ROOT/build/arm-debug/$PROJECT_NAME"      # 本地可执行文件路径
EXEC_NAME=$(basename "$LOCAL_EXECUTABLE")                           # 获取可执行文件名

echo "开始部署可执行文件到 $REMOTE_SERVER ..."
ssh $REMOTE_SERVER "mkdir -p $REMOTE_DIR"
scp "$LOCAL_EXECUTABLE" $REMOTE_SERVER:$REMOTE_DIR/

echo "在远程服务器上运行程序..."
ssh -t $REMOTE_SERVER "source /etc/profile && $REMOTE_DIR/$EXEC_NAME"