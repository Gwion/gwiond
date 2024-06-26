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
OBJ := $(SRC:.c=.o)

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

.c.o:
	$(info compile $(<:.c=))
	@${CC} $(DEPFLAGS) ${CFLAGS} -c $< -o $(<:.c=.o)
	@mv -f $(DEPDIR)/$(@F:.o=.Td) $(DEPDIR)/$(@F:.o=.d) && touch $@
	@echo $@: config.mk >> $(DEPDIR)/$(@F:.o=.d)

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$(@F:.o=.Td)

define _options
  $(info CC      : ${CC})
  $(info CFLAGS  : ${CFLAGS})
  $(info LDFLAGS : ${LDFLAGS})
endef

