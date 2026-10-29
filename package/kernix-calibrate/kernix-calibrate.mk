KERNIX_CALIBRATE_VERSION = 1
KERNIX_CALIBRATE_SITE = $(BR2_EXTERNAL_KERNIX_PATH)/ui
KERNIX_CALIBRATE_SITE_METHOD = local
KERNIX_CALIBRATE_DEPENDENCIES = sqlite mpg123 dbus

define KERNIX_CALIBRATE_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra -I$(STAGING_DIR)/usr/include/dbus-1.0 -I$(STAGING_DIR)/usr/lib/dbus-1.0/include $(TARGET_LDFLAGS) -o $(@D)/kernix-bluetooth-pair $(@D)/bluetooth-pair.c -ldbus-1
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra $(TARGET_LDFLAGS) -o $(@D)/kernix-library-scan $(@D)/library-scan.c $(@D)/library.c -lsqlite3
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra $(TARGET_LDFLAGS) -o $(@D)/kernix-calibrate $(@D)/calibrate.c $(@D)/library.c $(@D)/backend.c -lsqlite3 -pthread
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra $(TARGET_LDFLAGS) -o $(@D)/kernix-display-test $(@D)/test.c
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c99 -Wall -Wextra $(TARGET_LDFLAGS) -o $(@D)/kernix-lcd-registers $(@D)/lcd-probe.c
endef

define KERNIX_CALIBRATE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/kernix-bluetooth-pair $(TARGET_DIR)/usr/bin/kernix-bluetooth-pair
	$(INSTALL) -D -m 0755 $(@D)/kernix-library-scan $(TARGET_DIR)/usr/bin/kernix-library-scan
	$(INSTALL) -D -m 0755 $(@D)/kernix-calibrate $(TARGET_DIR)/usr/bin/kernix-calibrate
	$(INSTALL) -D -m 0755 $(@D)/kernix-display-test $(TARGET_DIR)/usr/bin/kernix-display-test
	$(INSTALL) -D -m 0755 $(@D)/kernix-lcd-registers $(TARGET_DIR)/usr/bin/kernix-lcd-registers
endef

$(eval $(generic-package))
