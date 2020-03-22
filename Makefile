MAKE_INSTALL_PREFIX = ./install
ITEM :=
# target marcros
TARGET_A := libwotsen_task.a
TARGET_SO := libwotsen_task.so
DEMO := demo
MAIN_SRC := demo.cpp

# compile marcros
DIRS := src
OBJS := 

# intermedia compile marcros
ALL_OBJS := 
CLEAN_FILES := $(DEMO) $(OBJS) $(TARGET_A) $(TARGET_SO)
DIST_CLEAN_FILES := $(OBJS)

# recursive wildcard
rwildcard=$(foreach d,$(wildcard $(addsuffix *,$(1))),$(call rwildcard,$(d)/,$(2))$(filter $(subst *,%,$(2)),$(d)))

# default target
default: show-info all

# non-phony targets
$(DEMO): build-subdirs $(OBJS) find-all-objs
	@echo -e "\t" CC $@
	@$(CC) $(ALL_OBJS) $(MAIN_SRC) -o $@ $(CCFLAG)

$(TARGET_A): build-subdirs $(OBJS) find-all-objs
	@echo -e "\t" CC $@
	@$(AR) $@ $(ALL_OBJS)

$(TARGET_SO): build-subdirs $(OBJS) find-all-objs
	@echo -e "\t" CC $@
	@$(SHARED) $(ALL_OBJS) -o $@

# phony targets
.PHONY: all
all: $(DEMO) $(TARGET_A) $(TARGET_SO)
	@echo Target $(TARGET) build finished.

.PHONY: clean
clean: clean-subdirs
	@echo CLEAN $(CLEAN_FILES)
	@rm -f $(CLEAN_FILES)

.PHONY: distclean
distclean: clean-subdirs
	@echo CLEAN $(DIST_CLEAN_FILES)
	@rm -f $(DIST_CLEAN_FILES)

# phony funcs
.PHONY: find-all-objs
find-all-objs:
	$(eval ALL_OBJS += $(call rwildcard,$(DIRS),*.o))

.PHONY: show-info
show-info:
	@echo Building Project

.PHONY: install
install:
	mkdir $(MAKE_INSTALL_PREFIX)/include/task/ -p
	mkdir $(MAKE_INSTALL_PREFIX)/lib/ -p
	cp $(TARGET_A) $(MAKE_INSTALL_PREFIX)/lib/ -f
	cp $(TARGET_SO) $(MAKE_INSTALL_PREFIX)/lib/ -f
	cp src/task.h src/task_utils.h $(MAKE_INSTALL_PREFIX)/include/task/ -f

# need to be placed at the end of the file
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
export ROOT_DIR := $(shell pwd)
export PROJECT_PATH := $(patsubst %/,%,$(dir $(mkfile_path)))
export MAKE_INCLUDE=$(PROJECT_PATH)/mkconfig/make.global
export SUB_MAKE_INCLUDE=$(PROJECT_PATH)/mkconfig/submake.global
include $(MAKE_INCLUDE)
