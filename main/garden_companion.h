#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { int year, month, day, hour, minute; uint8_t field; } garden_clock_edit_t;
#define GARDEN_CYCLE_END 2040u
uint8_t garden_companion_stage(uint32_t total_cleared);
uint8_t garden_companion_legacy_stage(uint32_t total_cleared);
uint8_t garden_companion_stage_with_floor(uint32_t total_cleared, uint8_t stage_floor);
uint32_t garden_companion_threshold(uint8_t stage);
const char *garden_companion_name(uint8_t stage);
uint8_t garden_companion_name_columns(uint8_t stage);
uint32_t garden_companion_next(uint32_t total_cleared);
uint32_t garden_companion_next_with_floor(uint32_t total_cleared, uint8_t stage_floor);
bool garden_companion_cycle_complete(uint32_t total_cleared);
uint8_t garden_companion_scale_tenths(uint8_t stage);
uint8_t garden_preview_stage_step(uint8_t stage, int direction);
bool garden_clock_valid(const garden_clock_edit_t *clock);
void garden_clock_adjust(garden_clock_edit_t *clock, int direction);
void garden_clock_next_field(garden_clock_edit_t *clock);
