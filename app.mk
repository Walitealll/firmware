include Makefile

# Пустое определение, как #define в заголовке. Голый -D задал бы макрос равным 1 и дал бы warning на каждом файле.
# После генерации Cube строка в заголовке снова окажется в комментарии, флаг её удержит.
C_DEFS += -DHAL_TIM_MODULE_ENABLED=

APP_C_SOURCES := \
App/FW_states.c \
App/FW_init.c \
App/FW_ble.c \
App/FW_protocol.c \
App/FW_emg.c \
App/FW_angle.c \
App/FW_sync.c \
App/FW_battery.c \
App/FW_diagnostics.c \
Core/Src/tim.c

# Снимок нужен: += с $(C_SOURCES) внутри filter-out зацикливает рекурсивную переменную.
# filter-out сравнивает путь целиком, поэтому Core/Src/tim.c не совпадает с hal_tim.c.
_have_c_sources := $(C_SOURCES)
C_SOURCES += $(filter-out $(_have_c_sources),$(APP_C_SOURCES))

_have_c_includes := $(C_INCLUDES)
C_INCLUDES += $(filter-out $(_have_c_includes),-IApp -Iconfig)

vpath %.c App

# Список объектов у elf раскрывается при чтении Makefile, ещё до добавлений выше.
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
