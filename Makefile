run:
	@cmake -S . -B build && cd build && make && ./stratax

fmt:
	@find src/ -iname '*.h' -o -iname '*.c' | xargs clang-format -i

clangd-file:
	@rm -rf build
	@mkdir build
	@cmake -S . -B build
	@bear -- cmake --build build

