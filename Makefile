all: test
.PHONY: all

.PHONY: test
test: consumers_and_producers_test
	./$<

%: %.cpp
	g++ --std=c++11 $$(gtest-config --cppflags --cxxflags --ldflags --libs) -o $@ $?
