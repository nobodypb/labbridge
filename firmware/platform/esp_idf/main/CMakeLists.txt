set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
file(GLOB CORE "${ROOT}/core/src/*.cpp")
file(GLOB DL "${ROOT}/drivers/dl24m/*.cpp")
file(GLOB RD "${ROOT}/drivers/rd60xx/*.cpp")
set(REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_server json bt esp_driver_gpio esp_driver_uart esp_timer esp_system esp_app_format esp_rom lwip)
if(IDF_TARGET STREQUAL "esp32s3")
 list(APPEND REQUIRES usb)
endif()
idf_component_register(SRCS "main.cpp" "network.cpp" "console.cpp" "ble.cpp" "usb.cpp" "gpio.cpp" ${CORE} ${DL} ${RD}
 INCLUDE_DIRS "." "${ROOT}/core/include" "${ROOT}/drivers/dl24m" "${ROOT}/drivers/rd60xx" "${ROOT}/generated"
 REQUIRES ${REQUIRES}
 EMBED_TXTFILES "../../../../web/dist/index.html" "../../../../web/dist/app.js")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
if(IDF_TARGET STREQUAL "esp32s3")
 target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=cdc_acm_host_open_v1_dispatch")
endif()
