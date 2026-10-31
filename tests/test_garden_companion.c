#include "garden_companion.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const uint32_t thresholds[7] = {0,60,180,360,600,960,1440};
    for (int i=0;i<7;i++) {
        assert(garden_companion_stage(thresholds[i]) == i);
        if (i) assert(garden_companion_stage(thresholds[i]-1) == i-1);
        assert(garden_companion_next(thresholds[i]) == (i==6 ? 0 : thresholds[i+1]));
    }
    const uint32_t quotas[7] = {60,120,180,240,360,480,600};
    for (int i=0;i<6;i++) assert(thresholds[i+1]-thresholds[i] == quotas[i]);
    assert(GARDEN_CYCLE_END-thresholds[6] == quotas[6]);
    assert(garden_companion_scale_tenths(0) <= 6);
    assert(garden_companion_scale_tenths(6) > garden_companion_scale_tenths(5));
    assert(garden_companion_threshold(6) == 1440);
    assert(garden_companion_legacy_stage(326) == 6);
    assert(garden_companion_stage_with_floor(326, 6) == 6);
    assert(garden_companion_next_with_floor(326, 6) == 2040);
    assert(garden_companion_next_with_floor(1439, 6) == 2040);
    assert(garden_companion_next_with_floor(1440, 6) == 2040);
    assert(garden_companion_next_with_floor(2040, 6) == 0);
    assert(!garden_companion_cycle_complete(2039));
    assert(garden_companion_cycle_complete(2040));
    assert(garden_companion_stage_with_floor(61, 2) == 2);
    assert(garden_companion_next_with_floor(61, 2) == 360);
    assert(garden_companion_stage_with_floor(1440, 0) == 6);
    assert(garden_companion_name_columns(6) == 5);
    const char *const names[7]={"象芽","小耳朵","稳稳站","卷鼻子","一朵花","花冠象","七彩花冠象"};
    for(int i=0;i<7;i++) assert(strcmp(garden_companion_name(i),names[i])==0);
    assert(garden_companion_scale_tenths(1) >= garden_companion_scale_tenths(0) * 2);
    for (int i=2;i<7;i++) assert(garden_companion_scale_tenths(i) > garden_companion_scale_tenths(i-1));
    assert(garden_preview_stage_step(0, -1) == 0);
    for (int i=0;i<6;i++) assert(garden_preview_stage_step(i, 1) == i+1);
    assert(garden_preview_stage_step(6, 1) == 6);
    for (int i=1;i<=6;i++) assert(garden_preview_stage_step(i, -1) == i-1);
    garden_clock_edit_t c = {2028,2,29,23,59,0};
    assert(garden_clock_valid(&c));
    garden_clock_adjust(&c,1); assert(c.year==2029 && c.day==28);
    c.field=1; garden_clock_adjust(&c,-1); assert(c.month==1);
    c.field=3; garden_clock_adjust(&c,1); assert(c.hour==0);
    c.field=4; garden_clock_adjust(&c,1); assert(c.minute==0);
    garden_clock_next_field(&c); assert(c.field==0);
    puts("companion and clock model: PASS");
}
