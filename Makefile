all:
	flex -o ./src/lexer.yy.c ./src/lexer.l
	gcc ./src/lexer.yy.c -o ./src/lexer -lfl
	./src/lexer < mock/ex2.domus

clean:
	rm -f lexer lexer.yy.c