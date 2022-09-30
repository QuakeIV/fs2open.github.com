
.PHONY: fs2open
fs2open:
	cmake -G Ninja -B build/ .
	cd build/; ninja
