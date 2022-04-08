# Resql database system and Flounder Library.
# author: Henning Funke <henning.funke@cs.tu-dortmund.de>

args= -march=native -std=c++20 -pthread -W -fPIC -Wall -Iasmjit/src \
	-Wno-unused-function           \
	-Wno-unused-parameter          \
	-Wno-write-strings

ifeq ($(DEBUG), 1)
	args := -g -O0 $(args)
else 
	args := -O3 -DNDEBUG $(args)
endif

resql-files = \
	src/resql.cpp \
	src/schema.h \
	src/debug.h \
	src/ExpressionsJitFlounder.h \
	src/types.h \
	src/ValuesJitFlounder.h \
        src/operators/RelOperator.h \
	src/operators/JitOperators.h \
	src/operators/scan.h \
	src/operators/aggregation.h \
	src/operators/hashjoin.h \
	src/operators/materialize.h \
	src/operators/nestedloopsjoin.h \
	src/operators/orderby.h \
	src/operators/projection.h \
	src/operators/selection.h \
	src/dbdata.h \
        src/expressions.h \
	src/JitContextFlounder.h \
	src/RelationalContext.h \
	src/schema.h \
	src/values.h \
	src/planner.h \
	src/execute.h \
	src/qlib/error.h \
	src/qlib/hash.h \
	src/qlib/sort.h \
	src/qlib/qlib.h \
	src/qlib/scalar.h \
        src/network/client.h \
        src/network/server.h \
        src/network/handler.h \
        src/network/def.h \
        src/util/assertion.h \
        src/util/Timer.h \
        src/util/string.h \
	src/parser/parseSql.h \

resql-parser-files = \
	src/parser/parser.y \
	src/parser/lexer.y \
	src/parser/parseSql.h \

resql-test-files = \
	test/test_datatypes.h \
	test/test_expressions.h \
	test/test_operators.h \
	test/test_queries.h \
	test/test.cpp \
	test/test_common.h \

flounder-files = \
	src/flounder/flounder_lang.h \
	src/flounder/asm_lang_simd.h \
	src/flounder/asm_emitter.h \
	src/flounder/ir_base.h \
	src/flounder/translate_analyze.h \
	src/flounder/translate.h \
	src/flounder/translate_vregs.h \
	src/flounder/asm_lang.h \
	src/flounder/flounder_constructs.h \
	src/flounder/flounder.h \
	src/flounder/RegisterAllocationState.h \
	src/flounder/translate_call.h \
	src/flounder/translate_optimize.h \
	src/flounder/x86_abi.h \

include-dirs = \
        -Ilib/cereal/include \
        -Ilib/cxxopts/ \
        -Ilib/asmjit/src/ \
        -Isrc/ \
        -I./ \

all: resql test-resql

resql: ${resql-files} ${flounder-files} resql-parser.o
	clang++ ${args} ${include-dirs} -Llib/asmjit -lasmjit \
            -L/usr/share -lreadline -Wl,--export-dynamic,-R./asmjit \
            src/resql.cpp -o resql resql-parser.o -ll 

resql-parser.o: ${resql-parser-files}
	cd lib/lemon && [ -f "lemon" ] || cc -o lemon lemon.c
	lib/lemon/lemon src/parser/parser.y -d.
	flex -o lexer.c src/parser/lexer.y;
	clang -c -O3 -I. -Isrc/ lexer.c -o resql-parser.o

test-resql: ${resql-test-files} ${resql-files} resql-parser.o
	clang++ ${args} ${include-dirs} -Isrc/ -Llib/asmjit -lasmjit test/test.cpp \
        -Wl,--export-dynamic,-R./asmjit -o test-resql resql-parser.o -ll

run-tests: test-resql
	./test-resql

clean:
	rm -f *.o jitnasm* test-resql resql lexer.c parser.h parser.c parser.out qres.tbl
