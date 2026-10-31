#include "garden_companion.h"
static const uint32_t thresholds[7] = {0, 60, 180, 360, 600, 960, 1440};
static const char *const names[7] = {"象芽", "小耳朵", "稳稳站", "卷鼻子", "一朵花", "花冠象", "七彩花冠象"};
static const uint8_t name_columns[7] = {2, 3, 3, 3, 3, 3, 5};
uint32_t garden_companion_threshold(uint8_t stage) { return thresholds[stage < 7 ? stage : 6]; }
const char *garden_companion_name(uint8_t stage) { return names[stage < 7 ? stage : 6]; }
uint8_t garden_companion_name_columns(uint8_t stage) { return name_columns[stage < 7 ? stage : 6]; }
uint8_t garden_companion_stage(uint32_t total_cleared) {
    uint8_t stage = 0;
    for (uint8_t i = 1; i < 7; ++i) if (total_cleared >= thresholds[i]) stage = i;
    return stage;
}
uint8_t garden_companion_legacy_stage(uint32_t total_cleared) {
    static const uint32_t old_thresholds[7] = {0, 24, 60, 108, 168, 240, 324};
    uint8_t stage = 0;
    for (uint8_t i = 1; i < 7; ++i) if (total_cleared >= old_thresholds[i]) stage = i;
    return stage;
}
uint8_t garden_companion_stage_with_floor(uint32_t total_cleared, uint8_t stage_floor) {
    uint8_t stage = garden_companion_stage(total_cleared);
    if (stage_floor > 6) stage_floor = 6;
    return stage > stage_floor ? stage : stage_floor;
}
uint32_t garden_companion_next_with_floor(uint32_t total_cleared, uint8_t stage_floor) {
    uint8_t stage = garden_companion_stage_with_floor(total_cleared, stage_floor);
    return stage == 6 ? 0 : thresholds[stage + 1];
}
uint32_t garden_companion_next(uint32_t total_cleared) {
    uint8_t stage = garden_companion_stage(total_cleared);
    return stage == 6 ? 0 : thresholds[stage + 1];
}
uint8_t garden_companion_scale_tenths(uint8_t stage) {
    static const uint8_t scales[7] = {6, 12, 14, 16, 18, 20, 25};
    return scales[stage < 7 ? stage : 6];
}
uint8_t garden_preview_stage_step(uint8_t stage, int direction) {
    if(stage < 1 || stage > 6) stage = 1;
    if(direction < 0 && stage > 1) return stage - 1;
    if(direction > 0 && stage < 6) return stage + 1;
    return stage;
}
static bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
static int days(int y, int m) {
    static const int lengths[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return lengths[m - 1] + (m == 2 && leap(y));
}
bool garden_clock_valid(const garden_clock_edit_t *c) {
    return c && c->year >= 2020 && c->year <= 2099 && c->month >= 1 && c->month <= 12 &&
           c->day >= 1 && c->day <= days(c->year, c->month) &&
           c->hour >= 0 && c->hour <= 23 && c->minute >= 0 && c->minute <= 59;
}
void garden_clock_adjust(garden_clock_edit_t *c, int direction) {
    if (!c || (direction != 1 && direction != -1) || c->field > 4) return;
    int *value = c->field == 0 ? &c->year : c->field == 1 ? &c->month :
                 c->field == 2 ? &c->day : c->field == 3 ? &c->hour : &c->minute;
    int lo = c->field == 0 ? 2020 : c->field == 1 || c->field == 2 ? 1 : 0;
    int hi = c->field == 0 ? 2099 : c->field == 1 ? 12 : c->field == 2 ? days(c->year,c->month) : c->field == 3 ? 23 : 59;
    *value += direction;
    if (*value > hi) *value = lo;
    if (*value < lo) *value = hi;
    if (c->field == 0 || c->field == 1) {
        int max_day = days(c->year,c->month);
        if (c->day > max_day) c->day = max_day;
    }
}
void garden_clock_next_field(garden_clock_edit_t *c) {
    if (c) c->field = (uint8_t)((c->field + 1) % 5);
}
