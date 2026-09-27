zephyr_include_directories(include)
zephyr_library()

if(CONFIG_SHIELD_DONGLE_SCREEN)

        add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR}/drivers/display)
        add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR}/modules/lvgl)


endif()

if(CONFIG_ZMK_SPLIT_CHARGING)
        zephyr_library_named(zmk_split_charging)
        zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
        zephyr_library_sources(src/split_charging_event.c)
        if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
                zephyr_library_sources(src/split_charging_central.c)
        else()
                zephyr_library_sources(src/split_charging_peripheral.c)
        endif()
endif()

if(CONFIG_ZMK_USB_1200_BAUD_BOOTLOADER)
        zephyr_library_named(zmk_usb_1200_baud_bootloader)
        zephyr_library_sources(src/usb_1200_baud_bootloader.c)
endif()
