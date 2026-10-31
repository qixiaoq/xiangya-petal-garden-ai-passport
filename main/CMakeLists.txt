idf_component_register(
    SRCS "main.c" "garden_model.c" "garden_companion.c" "../assets/fonts/garden_name_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support nvs_flash
)
