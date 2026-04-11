include Utilities/Configurations/Custom.mk
include Utilities/Configurations/Defaults.mk
include Utilities/Configurations/${BUILD_CONFIGURATION}.mk

.SUFFIXES:
.DEFAULT_GOAL := all

.PHONY: all

SOURCE_FILES := $(shell find . -name "*.cpp")
OBJECT_FILES := $(patsubst ./%.cpp, $(BUILD_OBJECT_DIRECTORY)/%.o, $(SOURCE_FILES))
DEPENDENCY_FILES := $(patsubst ./%.cpp, $(BUILD_OBJECT_DIRECTORY)/%.d, $(SOURCE_FILES))

all: $(OBJECT_FILES)
	@echo "Linking object files..."
	@mkdir -p $(BUILD_DIRECTORY)
 	@$(BUILD_TOOLCHAIN) $(BUILD_FLAGS) $(BUILD_OPTIMSATIONS) -o $(BUILD_DIRECTORY)/CelestiaNova $(OBJECT_FILES) $(THIRD_PARTY_LIBRARIES)

$(BUILD_OBJECT_DIRECTORY)/%.d: ./%.cpp
	@mkdir -p $(dir $@)
	@$(BUILD_TOOLCHAIN) $(BUILD_FLAGS) $(BUILD_OPTIMSATIONS) -MM $< -MT $(BUILD_OBJECT_DIRECTORY)/$(patsubst %.cpp,%.o,$(subst ./,,$<)) -MF $@
	@$(BUILD_TOOLCHAIN) $(BUILD_FLAGS) $(BUILD_OPTIMSATIONS) -c $< -o $@

-include $(DEPENDENCY_FILES)


$(BUILD_OBJECT_DIRECTORY)/%.d: ./%.cpp
	@mkdir -p $(dir $@)
	@$(BUILD_TOOLCHAIN) $(BUILD_FLAGS) $(BUILD_OPTIMSATIONS) -MM $< -MT $(patsubst ./%.cpp, $(BUILD_OBJECT_DIRECTORY)/%.o, $<) -MF $@

clean:
	@echo "Cleaning object files..."
	@rm -rf $(BUILD_OBJECT_DIRECTORY)
	@echo "Cleaning binaries..."
	@rm -rf $(BUILD_DIRECTORY)
