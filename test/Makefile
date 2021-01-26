
CFLAGS=-ggdb -O2 -fsanitize=address -lasan -ftest-coverage -fprofile-arcs

all:
	g++ -o util_test.bin util_test.cc ../util.cc -I .. $(CFLAGS)
	./util_test.bin
	lcov -c -d . -o util_test.info

	g++ -o executor_test.bin executor_test.cc -I .. $(CFLAGS)
	./executor_test.bin
	lcov -c -d . -o executor_test.info

	lcov -a executor_test.info -a util_test.info -o total.info
	rm -rf coverage/
	genhtml -o coverage/ total.info

clean:
	@rm -f *.info *.bin *.gcno *.gcda
	@rm -rf coverage/

