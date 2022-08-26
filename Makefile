CC      := g++
RM		:= del
CCFLAGS := -std=c++17 -g3 -Wno-attributes -DBUFFER_DEBUG
LDFLAGS := -g3
LIB     := lib
INCLUDE := 

TARGETS := test.exe
OBJ_FOLDER = obj
BIN_FOLDER = bin
#----------------------------------------

# $(1): folder
srcs_from = ${wildcard $(1)*.c} ${wildcard $(1)*.cpp}

# $(1): folder
deps_from = ${wildcard $(1)*.h} ${wildcard $(1)*.hpp}

# $(1): folder
glsl_from = ${wildcard $(1)*.glsl}

# converts a source file path to an obj path a/b\c.any becomes a.b.c.o
# $(1): source path
src_2_obj = $(OBJ_FOLDER)/${subst \,.,${subst /,.,${basename $(1)}}}.o

# finds the element in $1 which becomes $2 by applying $3
# $(1): list
# $(2): transformed
# $(3): transformation function
find_original = ${strip ${foreach elem,$(1),${if ${findstring $(2),${call $(3),$(elem)}}, $(elem)}}}

# $(1): list of prerequisites
# $(2): required .o file
build = $(CC) -c ${call find_original,$(1),$(2),src_2_obj} -o $(2) ${foreach v,$(INCLUDE),-I$(v)} $(CCFLAGS)

# $(1): list of sources
srcs_2_objs = ${foreach v,$(1),${call src_2_obj,$(v)}}

#----------------------------------------

SRC_COLD := ${call srcs_from,}
DEP_COLD := ${call deps_from,}
SRC_MAIN := ${call srcs_from,test/}
DEP_MAIN := ${call deps_from,test/}

SRC := $(SRC_MAIN) $(SRC_COLD)
DEP := $(DEP_MAIN) $(DEP_COLD)
OBJ := ${foreach v,$(SRC),${call src_2_obj,$(v)}}
BIN := ${foreach v,$(TARGETS),$(BIN_FOLDER)/$(v)}
#----------------------------------------

default: all

.PHONY: all clean work

all: $(BIN)

ifeq ($(RM),del)

clean:
	$(RM) /Q ${subst /,\,$(BIN)}
	$(RM) /Q ${subst /,\,$(OBJ_FOLDER)/*.o}

else

clean:
	$(RM) -f $(BIN) $(OBJ)

endif

#----------------------------------------

$(SRC_COLD): $(DEP_COLD)
${call srcs_2_objs,$(SRC_COLD)}: $(SRC_COLD)
	${call build,$^,$@}

$(SRC_MAIN): $(DEP_MAIN)
${call srcs_2_objs,$(SRC_MAIN)}: $(SRC_MAIN)
	${call build,$^,$@}

#----------------------------------------

$(BIN): $(OBJ)
	$(CC) -o $@ ${foreach v,$(LIB),-L$(v)} $^ $(CCFLAGS) $(LDFLAGS)
