# -----------------------------------------------------------------------------
# @file   Makefile
# @brief  Generic Makefile for the FSM editor project.
#
# Provides build targets for compiling the project, running the GUI application,
# executing tests, generating documentation, and creating submission packages.
# Configures CMake build with proper options and provides convenient shortcuts.
#
# @author Martin Ševčík (xsevcim00)
# @author Jakub Lůčný (xlucnyj00)
# @date   2025-05-06
# -----------------------------------------------------------------------------

# ---------- configurable knobs ---------------------------------------------
BUILD_DIR   ?= build
BIN_DIR     := $(BUILD_DIR)/bin
DOC_DIR     := doc
PACK_NAME   ?= submission            # override on cmd‑line if desired
CMAKE       ?= cmake
BUILD_TYPE  ?= Release               # switch to Debug with:  make BUILD_TYPE=Debug
JOBS        ?= $(shell nproc)        # parallelism for the native make inside CMake

# ---------------------------------------------------------------------------
#  Default target: compile everything
# ---------------------------------------------------------------------------
.PHONY: all build
all: build

build:
	@echo "\n==[ CMake configure ]================================================"
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@echo "\n==[ Build ]==========================================================="
	$(CMAKE) --build $(BUILD_DIR) -- -j$(JOBS)

# ---------------------------------------------------------------------------
#  Run the demo – spec requires a 'make run' convenience target
# ---------------------------------------------------------------------------
.PHONY: run
run: build
	@echo "\n==[ Launch GUI client ]=============================================="
	$(BIN_DIR)/gui_qt_client_exec

# ---------------------------------------------------------------------------
#  Doxygen documentation (requires a root‑level Doxyfile, SOURCE_BROWSER=YES)
# ---------------------------------------------------------------------------
.PHONY: doxygen
doxygen:
	@echo "\n==[ Doxygen ]========================================================="
	doxygen Doxyfile
	@echo "HTML documentation generated in $(DOC_DIR)/html"

# ---------------------------------------------------------------------------
#  Clean every generated file the spec tells us not to ship
# ---------------------------------------------------------------------------
.PHONY: clean
clean:
	@echo "\n==[ Clean ]==========================================================="
	rm -rf $(BUILD_DIR) $(DOC_DIR)/html

# ---------------------------------------------------------------------------
#  Pack everything for IS submission:
#    – removes build tree & docs
#    – yields $(PACK_NAME).zip in the current directory
# ---------------------------------------------------------------------------
.PHONY: pack
pack: clean
	@echo "\n==[ Creating submission archive ]===================================="
	zip -qr $(PACK_NAME).zip \
		. -x "$(BUILD_DIR)/*" "$(DOC_DIR)/html/*" "*.zip" "*.tar.gz"
	@echo "Wrote $(PACK_NAME).zip  - remember to rename to xsevcim00-xlucnyj00.zip"

# ---------------------------------------------------------------------------
#  Convenience phony to ensure goals that are *always* re‑evaluated
# ---------------------------------------------------------------------------
.PHONY: build run doxygen pack