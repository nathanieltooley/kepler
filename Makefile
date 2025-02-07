test: tests/tests.c kepler.h
	cc tests/tests.c -I . -lm -o test -Wall -Wextra -Werror -g 
