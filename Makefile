run:
	@cmake -S . -B build && cd build && make && ./stratax

fmt:
	@find src/ -iname '*.h' -o -iname '*.c' | xargs clang-format -i

