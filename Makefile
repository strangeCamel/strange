SRC_FILES:= $(wildcard src/*)
VERINFO:= $(shell git log -1 --format="%h %cd" || date)

strange: $(SRC_FILES)
	$(CXX) $(CXX_FLAGS) -std=c++17 -O2 -DVERINFO="\"$(VERINFO)\"" -Isrc src/strange.cpp -o strange #-DSTRINGS_INTERNING

clean:
	rm -f strange

install: strange scripts
	install strange /usr/local/bin/
	install scripts/strange-* /usr/local/bin/

.PHONY: clean

