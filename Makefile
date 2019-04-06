include platform.mk

warnings.cl := $(filter-out -WX, ${warnings.cl})

include rules.mk

all: ${tg.out}

clean:
	${hide}echo Cleaning build folder.
	${hide}${clean_build_dir}

run: all
	${hide}echo Running ${tg.out}.
	${hide}${tg.out} "local/data/invocation.tg"