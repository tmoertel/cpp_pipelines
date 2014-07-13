# Makefile rules for building and testing basic C++ code.
# To use the rules, create a Makefile defining your objects, tests,
# and any special link flags you need, and then include the rules:
#   objects = name_of_library.o...
#   tests = name_of_binary_test name_of_another_binary_test...
#   link_flags = -lprotobuf
#   include rules.mk

gtest_flags = $$(gtest-config --cppflags --cxxflags --ldflags --libs)
test_flags  = $(gtest_flags) -fprofile-arcs -ftest-coverage
link_flags := $(link_flags) -lgcov -lstdc++
cxx         = clang --std=c++11

obj_cc_files = $(objects:.o=.cc)

test: $(tests)
	for t in $?; do echo "### Running $t ###"; ./$$t; done
.PHONY: test

$(tests): $(objects)

.PHONY: coverage
coverage: $(tests) $(obj_cc_files)
	mkdir -p coverage
	lcov --directory . --zerocounters
	for t in $(tests); do echo "### Running $$t ###"; ./$$t; done
	lcov --directory . --capture --output-file coverage/test.info
	genhtml -o coverage coverage/test.info
	@echo "### Coverage report is in file://$$(pwd)/coverage/index.html"

.PHONY: clean
clean:
	rm -rf $(tests) $(objects) coverage *.gc{da,no}

%: %.cc
	$(cxx) $(test_flags) $(link_flags) -o $@ $< $(objects)

%.o: %.cc
	$(cxx) $(test_flags) -c -o $@ $?
