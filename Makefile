CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I.
LDFLAGS = -lgtest -lgtest_main -pthread

# 源文件和目标文件
TEST_SRCS = $(wildcard test_*.cpp)
TEST_OBJS = $(TEST_SRCS:.cpp=.o)
TEST_TARGETS = $(TEST_SRCS:.cpp=)

# 默认目标
all: $(TEST_TARGETS)

# 链接所有测试
$(TEST_TARGETS): %: %.o
	$(CXX) $< -o $@ $(LDFLAGS)

# 编译
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 运行所有测试
test: $(TEST_TARGETS)
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test; \
	done

# 运行特定测试
# 用法: make run_test TEST=test_deque
run_test: $(TEST_TARGETS)
	@if [ -z "$(TEST)" ]; then \
		echo "Error: Please specify a test to run using TEST=<test_name>"; \
		echo "Available tests: $(TEST_TARGETS)"; \
		exit 1; \
	fi
	@if [ ! -f "$(TEST)" ]; then \
		echo "Error: Test '$(TEST)' not found"; \
		echo "Available tests: $(TEST_TARGETS)"; \
		exit 1; \
	fi
	@echo "Running $(TEST)..."
	@./$(TEST)

# 生成 compile_commands.json
compile_commands:
	@chmod +x generate_compile_commands.sh
	@./generate_compile_commands.sh

# 清理
clean:
	rm -f $(TEST_OBJS) $(TEST_TARGETS)

.PHONY: all test run_test compile_commands clean 