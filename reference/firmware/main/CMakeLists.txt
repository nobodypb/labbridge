if(IDF_TARGET STREQUAL "esp32")
    set(LAB_TRANSPORT "transports_spp.cpp")
    set(LAB_REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_server json bt esp_driver_gpio esp_timer esp_system)
else()
    set(LAB_TRANSPORT "transports.cpp")
    set(LAB_REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_server json bt usb esp_driver_gpio esp_timer esp_system)
endif()
idf_component_register(SRCS "main.cpp" "network.cpp" "devices.cpp" "${LAB_TRANSPORT}" "serial.cpp" "../core/protocol.cpp" "../core/captive_dns.cpp"
    INCLUDE_DIRS "." "../core"
    EMBED_TXTFILES "setup.html"
    REQUIRES ${LAB_REQUIRES})
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
# Vendor VCP constructors use the legacy opener. Route it through an address-aware adapter
# so two converters with identical VID/PID can be opened independently.
if(NOT IDF_TARGET STREQUAL "esp32")
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=cdc_acm_host_open_v1_dispatch")
endif()
