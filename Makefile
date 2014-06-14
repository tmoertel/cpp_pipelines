gtest_flags = $$(gtest-config --cppflags --cxxflags --ldflags --libs)
test_flags  = $(gtest_flags) -fprofile-arcs -ftest-coverage -lgcov

all: test
.PHONY: all

.PHONY: test
test: consumers_and_producers_test
	./$<

.PHONY: coverage
coverage: consumers_and_producers_test
	mkdir -p coverage
	lcov --directory . --zerocounters
	./$<
	lcov --directory . --capture --output-file coverage/test.info
	genhtml -o coverage coverage/test.info
	@echo "### Coverage report is in file://$$(pwd)/coverage/index.html"

%: %.cpp
	g++ --std=c++11 $(test_flags) -o $@ $?

clean:
	rm -rf consumers_and_producers_test coverage *.gc{da,no}
