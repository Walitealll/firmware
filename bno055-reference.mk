# Separate diagnostic build. Normal app.mk and build/firmware.elf are unchanged.
include app.mk

C_SOURCES += diagnostics/bno055_reference.c
vpath %.c diagnostics

# Reuse generated SystemClock_Config/Error_Handler, but the diagnostic file
# supplies main. The unused original application entry is removed by the linker.
$(BUILD_DIR)/main.o: Core/Src/main.c Makefile bno055-reference.mk | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Dmain=firmware_application_main -Wa,-a,-ad,-alms=$(BUILD_DIR)/main.lst $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) bno055-reference.mk
