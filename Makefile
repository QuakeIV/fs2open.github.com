.PHONY: fs2open

fs2open:
	cmake -G Ninja -B build/ . -DCMAKE_BUILD_TYPE=Debug # TODO: get working: -DFSO_BUILD_WXFRED2=ON
	cd build/; ninja
