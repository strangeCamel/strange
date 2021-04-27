# Strange Tool
Tiny plain text anomaly detection tool intended to analyze log and similar files, written in C++.<br>
Fast and easy to use, provides colored output of anomalies with per-line or per-word granularity.<br>
Uses prefix-tree approach with adaptive patterns generation. No AI inside - just simple text processing.<br>
BSD 3-clause license.<br>

#### Build and install
 * Supported compiler: tested with GCC 9.3.0 but should work fine with any other C++17-capable compiler.
 * Can be compiled in C++14 too, but in this case performance will be degraded.
 * No extra dependencies - needs only C++-capable compiler and make.
 * To build, run: `make`
 * To test, run: `make test`
 * To install, run: `sudo make install`

##### Using
 There're three executable files will be installed into /usr/local/bin:
 * strange - its a main executable, run `strange --help` to get help on its possible command lines
 * strange-learn - helper script intended to learn files given to its command line: `strange-learn some_existing_file.log`
 (you can also feed multiple files at once). It will learn content of that files and will save learning results into ~/.config/strange/some_existing_file.trie - so when next file you will learn or eval same file name (path and extension don't matter) - it will reuse that results. But other file pathes will be sved separately.
 * strange-eval - helper script intended to evaluate files given to its command line: `strange-eval some_existing_file.log`
 (you can also feed multiple files at once). It will evaluate content of that files printing out any strange lines according to learned results loaded from ~/.config/strange/some_existing_file.trie. By default it produces results with line-level granularity, but you can enforce token-level granularity by adding -descript option argument before list of files.
 * Note that while this tool is in BETA stage, there is no efforts to keep trie backward compatibility. So for now tries created by older version may produce incorrect results when used with newer version (and vice verse).

##### How it works
During the learning each sample is splitted into tokens - short sequences of chars that have similar properties - either they all alphabetical, either numerical, etither punctuation etc.
Then that tokens used to build prefix-tree, each node of which represents value that particular token may have according to some learned sample.
Then tree is analyzed and some sibling nodes are 'converged' - currently its a nodes that having values representing inconstant numbers, dates and randomly-generated strings. Such nodes replaced with string-class node that matches to original nodes values plus any other but _similar_ value.
Another part of analyzis is to merge duplicated nodes into single ones that have coalesced nested subnodes that also deduplicated etc.
Eventually produced prefix tree saved to file and then may be loaded again and either incrementally updated with new samples either used to evaluate other input lines and produce result - if it looks similar or not to what was already seen during learning.
