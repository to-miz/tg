# Configurable summodule makefile for use with platform.mk.

# Configurable variables.
tg_build ?= ${BUILD}
tg_src ?= src/
tg_external ?= external/
tg_executable ?= tg

# Dependencies

tg_ucd_h := ${tg_src}tm_unicode_custom_ucd.h
tg_ucd_c := ${tg_src}tm_unicode_custom_ucd.c

# tg executable

tg.out := ${build_dir_root}${tg_build}${path_sep}${tg_executable}${exe_ext}
${tg.out}: private override BUILD := ${tg_build}
${tg.out}: private options.cl.exception := -EHs
${tg.out}: private warnings.gcc += -Wno-missing-field-initializers
${tg.out}: ${tg_src}*.cpp ${tg_src}*.h ${tg_external}tm/* ${tg_ucd_h} ${tg_ucd_c}
	${hide}echo Compiling $@.
	${hide}$(call cxx_compile_and_link, ${tg_src}main.cpp, $@, ${tg_external} ${tg_src})

# Generate unicode data dynamically, if generator is part of the build system.
# Otherwise the generated files should already be in the src folder, which will get automatically used.

ifneq (${unicode_gen.out},)

define tg_ucd_flags :=
case_info
category
grapheme_break
width
full_case
full_case_fold
simple_case
simple_case_fold
handle_invalid_codepoints
prune_stage_one
prune_stage_two
endef
tg_ucd_flags := $(subst ${newline},${comma},${tg_ucd_flags})

${tg_ucd_h} ${tg_ucd_c}: ${unicode_gen.out}
	${hide}echo Generating ${tg_ucd_h} and ${tg_ucd_c}.
	${hide}${unicode_gen.out} dir ${tg_external}tm/tools/unicode_gen/data --prefix=tmu_ --assert=TM_ASSERT \
		--flags=${tg_ucd_flags} \
		--output=${tg_ucd_c} --header=${tg_ucd_h}

endif