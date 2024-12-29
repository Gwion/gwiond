CFLAGS += -I../util/include
CFLAGS += -I../ast/include
CFLAGS += -I../fmt/include
CFLAGS += -I../include
CFLAGS += -Iinclude

#CFLAGS += -Wall
CFLAGS += -Wextra

LDFLAGS += -lcjson
LDFLAGS += ../libgwion.a
LDFLAGS += ../fmt/libgwion_fmt.a
LDFLAGS += ../ast/libgwion_ast.a
LDFLAGS += ../util/libgwion_util.a
LDFLAGS += -lm
LDFLAGS += -O3
LDFLAGS += -fwhole-program

SRC := $(wildcard src/*.c)
SRC += $(wildcard src/feat/*/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

.PHONY: clean options

all: options gwiond

gwiond: ${OBJ}
	$(info link $@)
	@${CC} -rdynamic -fPIC -o gwiond $^ $(CJSON) ${LDFLAGS}

clean:
	@rm -f gwiond ${OBJ}

options:
	$(call _options)	

install:
	install -m 777 gwiond /usr/bin

build/%.o: $(subst build,src, $(@:.o=.c))
	$(info compile $(subst build/,,$(@:.o=)))
	@mkdir -p $(shell dirname $@) > /dev/null
	@mkdir -p $(subst build,.d,$(shell dirname $@)) > /dev/null
	@${CC} $(DEPFLAGS) ${CFLAGS} -c $(subst build,src,$(@:.o=.c)) -o $@
	@mv -f $(subst build,${DEPDIR},$(@:.o=.Td)) $(subst build,${DEPDIR},$(@:.o=.d)) && touch $@

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(subst build,${DEPDIR},$(@:.o=.Td))
DEPS := $(subst build,$(DEPDIR),$(OBJ:.o=.d))
-include $(DEPS)


define _options
  $(info CC      : ${CC})
  $(info CFLAGS  : ${CFLAGS})
  $(info LDFLAGS : ${LDFLAGS})
  
  $(info DEPS : ${DEPS})
endef

