GCC=gcc 
INCLUDE=$(shell echo *.h)
EXEC = color 

%.o: %.c $(INCLUDE)
	${GCC} -c -o $@ $<

color: main.o utility.o http.o
	${GCC} -o $@ $^

clean:
	rm -f ${EXEC} *.o