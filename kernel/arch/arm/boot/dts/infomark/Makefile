ifeq ($(CONFIG_OF),y)

dtb-y += $(TARGET_PRODUCT).dtb

ifeq ($(TARGET_PRODUCT),if_s300n)
dtb-y += if_s300n_r2.dtb
dtb-y += if_s300n_r3.dtb
dtb-y += if_s300n_r4.dtb
dtb-y += if_s300n_r10.dtb
else ifeq ($(TARGET_PRODUCT),if_s301n)
dtb-y += if_s301n_r2.dtb
dtb-y += if_s301n_r3.dtb
dtb-y += if_s301n_r4.dtb
dtb-y += if_s301n_r10.dtb
else ifeq ($(TARGET_PRODUCT),if_s600n)
dtb-y += if_s600nl.dtb
dtb-y += if_s600nl_r2.dtb
dtb-y += if_s600n_r2.dtb
else ifeq ($(TARGET_PRODUCT),if_s600nl)
dtb-y += if_s600n.dtb
dtb-y += if_s600n_r2.dtb
dtb-y += if_s600nl_r2.dtb
endif


DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(DTB_LIST))

clean-files := *.dtb
