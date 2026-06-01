all: domus-transpiler

# Generated parser and lexer
src/parser.tab.c src/parser.tab.h: src/parser.y
	@echo "Running bison..."
	@bison -d -o src/parser.tab.c src/parser.y

src/lexer.yy.c: src/lexer.l src/parser.tab.h
	@echo "Running flex..."
	@flex -o src/lexer.yy.c src/lexer.l

domus-transpiler: src/parser.tab.c src/lexer.yy.c src/ast.c src/yaml.c
	@gcc -g -o domus-transpiler src/parser.tab.c src/lexer.yy.c src/ast.c src/yaml.c -lfl

run: domus-transpiler
	./domus-transpiler < examples/domus/ex1.domus > examples/output/out1.yaml

clean:
	rm -f domus-transpiler src/parser.tab.c src/parser.tab.h src/lexer.yy.c out1.yaml out2.yaml