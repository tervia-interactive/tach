# tach - Generic Build Rules
# Common rules for compiling C and Assembly files

# Rule to compile C files to object files
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile assembly files (.S) to object files
%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile assembly files (.asm) to object files (NASM syntax for x86)
%.o: %.asm
	@mkdir -p $(dir $@)
	nasm $(ASMFLAGS) $< -o $@

# Clean rule
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: clean
