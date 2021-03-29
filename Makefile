SRC_FILES:= $(wildcard src/*)

strange: $(SRC_FILES)
	$(CXX) -std=c++17 -O2  -Isrc src/strange.cpp -o strange #-DSTRINGS_INTERNING

clean:
	rm -f strange

install: strange scripts
	install strange /usr/local/bin/
	install scripts/strange-* /usr/local/bin/

.PHONY: clean

