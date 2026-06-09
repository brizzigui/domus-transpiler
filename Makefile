all: domus-transpiler

# Generated parser and lexer
src/parser.tab.c src/parser.tab.h: src/parser.y
	@echo "Running bison..."
	@bison -d -o src/parser.tab.c src/parser.y

src/lexer.yy.c: src/lexer.l src/parser.tab.h
	@echo "Running flex..."
	@flex -o src/lexer.yy.c src/lexer.l

domus-transpiler: src/parser.tab.c src/lexer.yy.c src/ast.c src/yaml.c src/symtab.c src/semantic.c
	@gcc -g -o domus-transpiler src/parser.tab.c src/lexer.yy.c src/ast.c src/yaml.c src/symtab.c src/semantic.c -lfl

run: domus-transpiler
	@mkdir -p examples/output
	@for f in examples/domus/ex*.domus; do \
		n=$$(basename $$f .domus); \
		echo "Generating $$n.yaml"; \
		./domus-transpiler < $$f > examples/output/$$n.yaml; \
	done

errors: domus-transpiler
	@for f in examples/domus/broken/*.domus; do \
		echo "--- Testing $$f ---"; \
		./domus-transpiler < $$f; \
		echo ""; \
	done

clean:
	rm -f domus-transpiler src/parser.tab.c src/parser.tab.h src/lexer.yy.c out1.yaml out2.yaml