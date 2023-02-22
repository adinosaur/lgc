# lgc

light gc system base reference-counting.

# Usage
```bash
# Use std::shared_ptr
make clean && USE_STD_SHARED_PTR=1 make
valgrind --tool=cachegrind ./a.out
# Use lgc::GCObjectPtr
make clean && make
valgrind --tool=cachegrind ./a.out
```