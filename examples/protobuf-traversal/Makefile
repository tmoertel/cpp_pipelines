objects = example.pb.o
tests = proto_accessors_test
include ../../rules.mk

link_flags := $(link_flags) -lprotobuf

proto_accessors_test : ../../consumers_and_producers.h
proto_accessors_test : ../../read_write_filters.h

# Rule to build C++ interfaces to protocol buffers.
%.pb.cc %.pb.h: %.proto
	protoc --cpp_out=. $<
