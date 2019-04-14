include platform.mk

all: ;

warnings.cl := $(filter-out -WX, ${warnings.cl})

# Unicode data generator
unicode_gen_build := release
unicode_gen_src := extern/tm/tools/unicode_gen/src/
unicode_gen_tm_root := extern/tm
include extern/tm/tools/unicode_gen/rules.mk

unicode_gen: ${unicode_gen.out};

# tg rules
include rules.mk
tg: ${tg.out};

# General

all: ${unicode_gen.out} ${tg.out}

clean:
	${hide}echo Cleaning build folder.
	${hide}${clean_build_dir}

run: all
	${hide}echo Running ${tg.out}.
	${hide}${tg.out} "local/data/invocation.tg"