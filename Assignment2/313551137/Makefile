.PHONY: all clean

# 使用 C++ 編譯器
CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -O3 -Wpedantic -pthread #add pthread
OUTPUT_OPTION = -MMD -MP -o $@

# 源文件和目標文件
SOURCE = sched_demo_313551137.cpp
OBJS = $(SOURCE:.cpp=.o)
DEPS = $(SOURCE:.cpp=.d)
TARGET = sched_demo_313551137 #without .out

# 默認目標
all: $(TARGET)

# 引入依賴文件
-include $(DEPS)

# 生成目標文件
$(TARGET): $(OBJS)
	$(CXX) $^ -o $@ $(CXXFLAGS) #what is CXXFLAGS

# 清理
clean:
	@rm -f $(TARGET) $(OBJS) $(DEPS)
