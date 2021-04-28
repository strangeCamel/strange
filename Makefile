SRC_FILES:= $(wildcard src/*)
VERINFO:= $(shell git log -1 --format="%h %cd" || date)
STRANGE:=_results/strange

$(STRANGE): $(SRC_FILES)
	mkdir -p _results
	$(CXX) $(CXX_FLAGS) -std=c++17 -O2 -DVERINFO="\"$(VERINFO)\"" -Isrc src/strange.cpp -o $(STRANGE) #-DSTRINGS_INTERNING

test: $(STRANGE) test/test.sh
	cd test && ./test.sh

clean:
	rm -rf _results

install: $(STRANGE) scripts
	install $(STRANGE) /usr/local/bin/
	install scripts/strange-* /usr/local/bin/

.PHONY: clean test
