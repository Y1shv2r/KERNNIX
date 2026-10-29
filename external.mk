# Kernix uses upstream Buildroot packages.
include $(sort $(wildcard $(BR2_EXTERNAL_KERNIX_PATH)/package/*/*.mk))
