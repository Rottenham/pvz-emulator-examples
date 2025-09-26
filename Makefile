CXX := g++
CXXFLAGS := -static -O3 -I. -isystem lib -isystem lib/lib -Llib/build -lpvzemu \
            -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion -Wctor-dtor-privacy \
            -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op \
            -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual \
            -Wredundant-decls -Wstrict-null-sentinel -Wswitch-default -Wswitch-enum \
            -Wsign-compare -Wsign-promo -Wundef -Wunused-variable -Wuseless-cast \
            -Wzero-as-null-pointer-constant -Wfatal-errors -Werror
# Source files (excluding those starting with an underscore or debug)
SOURCES := $(filter-out _% debug%, $(wildcard *.cpp))
# Object files
OBJECTS := $(SOURCES:%.cpp=%)
# Targets
TARGETS := $(foreach obj, $(OBJECTS), dest/bin/$(obj))

# Default target: build the library and then compile sources
default: buildLib $(TARGETS)

# Build the library
buildLib:
	@echo "Building library..."
	@cd lib/build && cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebugInfo .. && ninja && cd ../../

# Compile sources
dest/bin/%: %.cpp
	@echo "Compiling $<..."
	@$(CXX) $< -o $@ $(CXXFLAGS)
	@echo.

# Clean the build
clean:
	@echo "Cleaning..."
	@rm -f $(TARGETS)
	@cd lib/build && ninja clean

.PHONY: default buildLib clean