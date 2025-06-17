#!/bin/bash

# 设置工作目录
WORK_DIR=$(pwd)
OUTPUT_FILE="compile_commands.json"

# 获取所有的 .h 和 .cpp 文件
FILES=$(find . -type f \( -name "*.h" -o -name "*.cpp" \))

# 开始写入 compile_commands.json
echo "[" > $OUTPUT_FILE

FIRST=true
for FILE in $FILES; do
    # 去掉路径前面的 ./
    CLEAN_FILE=${FILE#./}
    
    # 添加逗号分隔（除了第一个条目）
    if [ "$FIRST" = true ]; then
        FIRST=false
    else
        echo "," >> $OUTPUT_FILE
    fi
    
    # 写入编译命令
    cat << EOF >> $OUTPUT_FILE
    {
        "directory": "$WORK_DIR",
        "command": "g++ -std=c++20 -Wall -Wextra -I. -c $CLEAN_FILE",
        "file": "$CLEAN_FILE"
    }
EOF
done

# 结束 JSON 文件
echo -e "\n]" >> $OUTPUT_FILE

echo "Generated $OUTPUT_FILE with $(grep -c "directory" $OUTPUT_FILE) entries." 