# Directories.
tg_src ?= src/
tg_extern ?= extern/
tg_executable ?= tg

tg.out ?= ${build_dir}${tg_executable}${exe_ext}

${tg.out}: private options.cl.exception := -EHs
${tg.out}: private warnings.gcc += -Wno-missing-field-initializers
${tg.out}: ${tg_src}*.cpp ${tg_src}*.h ${tg_extern}tm/*
	${hide}echo Compiling $@.
	${hide}$(call cxx_compile_and_link, ${tg_src}main.cpp, $@, ${tg_extern} ${tg_src})