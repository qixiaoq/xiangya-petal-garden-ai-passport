// Petal Garden: offline, untimed match-three with a growing companion and local clock.
#include "garden_model.h"
#include "garden_companion.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define INPUT_QUEUE_DEPTH 8
#define SAVE_VERSION 3u
#define BOARD_X 27
#define BOARD_Y 112
#define TILE 31
#define PET_PARTS 48

LV_FONT_DECLARE(garden_name_16);
static const char *TAG = "petal_garden";
static const uint32_t FLOWER[6] = {0,0xF292A5,0xA895DD,0xF7C56A,0x8CC6BD,0xEBAE83};
static const uint32_t PET_SKIN[7] = {0xC5BFD5,0xBDB9D6,0xB6B6D5,0xB0B2D4,0xAEB1D8,0xA9ACD4,0xA4A5D0};
static const uint32_t PET_INK = 0x6D697A;
static garden_game_t s_game;
static QueueHandle_t s_inputs;
static volatile bool s_ready;
static lv_obj_t *s_tiles[36];
static lv_obj_t *s_pet_root, *s_pet_parts[PET_PARTS], *s_pet_name;
static lv_obj_t *s_level, *s_progress, *s_target, *s_date, *s_time, *s_status, *s_battery;
static lv_obj_t *s_clock_view, *s_clock_rows[5], *s_clock_values[5];
static garden_clock_edit_t s_clock_edit;
static bool s_clock_editing, s_clock_valid, s_sound_ready, s_sound_enabled = true;
static bool s_preview_mode;
static uint8_t s_preview_stage = 1;
static uint8_t s_cursor, s_first = 255, s_neighbor = 255;

typedef struct { uint32_t version; garden_game_t game; } saved_game_t;
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } input_t;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_radius(obj,0,0);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(obj,0,0);
    lv_obj_set_style_pad_all(obj,0,0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,const char *text,uint32_t color) {
    lv_obj_t *obj=lv_label_create(parent);
    lv_obj_set_pos(obj,x,y);
    lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_font(obj,&lv_font_montserrat_14,0);
    lv_label_set_text(obj,text);
    return obj;
}
static void pet_part(int *count,int x,int y,int w,int h,uint32_t color,int scale,int offset) {
    if (*count >= PET_PARTS) return;
    lv_obj_t *part=s_pet_parts[(*count)++];
    lv_obj_set_pos(part,offset+(x*scale+5)/10,(64-24*scale/10)/2+(y*scale+5)/10);
    lv_obj_set_size(part,(w*scale+9)/10,(h*scale+9)/10);
    lv_obj_set_style_bg_color(part,lv_color_hex(color),0);
    lv_obj_remove_flag(part,LV_OBJ_FLAG_HIDDEN);
}
static void pet_flower(int *count,int x,int y,int size,int scale,int offset) {
    pet_part(count,x+size,y,size,size,0xF292A5,scale,offset);
    pet_part(count,x,y+size,size,size,0xF292A5,scale,offset);
    pet_part(count,x+2*size,y+size,size,size,0xF292A5,scale,offset);
    pet_part(count,x+size,y+2*size,size,size,0xF292A5,scale,offset);
    pet_part(count,x+size,y+size,size,size,0xF7D47B,scale,offset);
}
static void render_pet(uint8_t stage) {
    int n=0, scale=garden_companion_scale_tenths(stage), offset=(60-24*scale/10)/2;
    uint32_t skin=PET_SKIN[stage];
#define P(x,y,w,h,c) pet_part(&n,x,y,w,h,c,scale,offset)
    if (stage==0) {
        P(6,8,12,11,PET_INK); P(7,7,10,13,skin);
        P(4,10,4,7,PET_INK); P(4,11,3,5,0xE9AEBF);
        P(17,10,4,7,PET_INK); P(18,11,3,5,0xE9AEBF);
        P(11,16,3,5,PET_INK); P(12,16,2,5,skin);
        P(9,12,2,2,PET_INK); P(15,12,2,2,PET_INK);
        P(10,5,3,3,0x6EAA7A);
    } else {
        int width=stage==1?12:stage==2?14:16, x=(24-width)/2;
        P(x-1,15,width+2,8,PET_INK); P(x,14,width,8,skin);
        P(x+1,20,4,4,PET_INK); P(x+width-5,20,4,4,PET_INK);
        P(4,7,16,13,PET_INK); P(5,6,14,13,skin);
        int ear=stage==1?5:7;
        P(2,8,ear,10,PET_INK); P(3,9,ear-2,8,0xE9AEBF);
        P(22-ear,8,ear,10,PET_INK); P(23-ear,9,ear-2,8,0xE9AEBF);
        P(8,11,2,2,PET_INK); P(15,11,2,2,PET_INK);
        P(6,15,2,2,0xEEA6A7); P(17,15,2,2,0xEEA6A7);
        P(10,15,5,7,PET_INK); P(11,15,3,6,skin);
        if(stage>=3){ P(12,20,6,3,PET_INK); P(12,20,5,2,skin); }
        if(stage>=2){ P(5,5,3,3,0x6EAA7A); P(7,4,3,2,0x6EAA7A); }
        if(stage>=4)pet_flower(&n,9,0,2,scale,offset);
        if(stage>=5){ pet_flower(&n,2,2,1,scale,offset); pet_flower(&n,16,3,1,scale,offset); }
        if(stage>=6){ pet_flower(&n,1,0,1,scale,offset); pet_flower(&n,19,0,1,scale,offset); }
    }
#undef P
    for(int i=n;i<PET_PARTS;++i)lv_obj_add_flag(s_pet_parts[i],LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_pet_root);
}
// Draw five square petals without allocating five child objects per tile.
static void draw_flower(lv_event_t *event) {
    int index=(int)(intptr_t)lv_event_get_user_data(event);
    uint8_t flower=s_game.cells[index];
    if(!flower)return;
    lv_obj_t *tile=lv_event_get_target_obj(event);
    lv_area_t tile_area;lv_obj_get_coords(tile,&tile_area);
    lv_layer_t *layer=lv_event_get_layer(event);
    static const int dx[5]={12,5,19,12,12};
    static const int dy[5]={5,12,12,19,12};
    for(int i=0;i<5;++i){
        lv_draw_rect_dsc_t dsc;lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color=lv_color_hex(i==4?0xF7D47B:FLOWER[flower]);
        lv_area_t rect={tile_area.x1+dx[i],tile_area.y1+dy[i],
                        tile_area.x1+dx[i]+6,tile_area.y1+dy[i]+6};
        lv_draw_rect(layer,&dsc,&rect);
    }
}
static void refresh_clock(void) {
    if(!s_clock_valid){lv_label_set_text(s_time,"--:--");lv_label_set_text(s_date,"----/--/--");return;}
    time_t now=time(NULL); struct tm local;
    if(!localtime_r(&now,&local)){lv_label_set_text(s_time,"--:--");lv_label_set_text(s_date,"----/--/--");return;}
    char line[20];
    strftime(line,sizeof(line),"%H:%M",&local);lv_label_set_text(s_time,line);
    strftime(line,sizeof(line),"%Y/%m/%d",&local);lv_label_set_text(s_date,line);
}
static void clock_tick(lv_timer_t *timer) { (void)timer; refresh_clock(); }
static void refresh_clock_edit(void) {
    const int values[5]={s_clock_edit.year,s_clock_edit.month,s_clock_edit.day,s_clock_edit.hour,s_clock_edit.minute};
    for(int i=0;i<5;++i){
        char line[12];lv_snprintf(line,sizeof(line),i==0?"%04d":"%02d",values[i]);
        lv_label_set_text(s_clock_values[i],line);
        lv_obj_set_style_bg_color(s_clock_rows[i],lv_color_hex(i==s_clock_edit.field?0xF7E9BD:0xD7E3CC),0);
    }
}
static void build_ui(void) {
    lv_obj_t *screen=lv_obj_create(NULL);
    lv_obj_set_size(screen,240,320);lv_obj_set_style_bg_color(screen,lv_color_hex(0xE7E0C8),0);
    lv_obj_set_style_border_width(screen,0,0);lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    box(screen,0,0,240,108,0xA9C4A4);box(screen,0,104,240,4,0x779A73);
    label(screen,25,7,"PETAL",0x483D3C);
    s_time=label(screen,178,6,"--:--",0x483D3C);
    s_date=label(screen,13,87,"----/--/--",0x483D3C);
    s_level=label(screen,13,28,"LEVEL 1/7",0x483D3C);
    box(screen,13,50,120,7,0xEDE6D2);s_progress=box(screen,13,50,2,7,0xF292A5);
    s_target=label(screen,13,63,"0/24 FLOWERS",0x526B53);
    s_pet_root=box(screen,160,20,60,64,0xA9C4A4);
    s_pet_name=label(screen,176,87,"象芽",0x483D3C);
    lv_obj_set_style_text_font(s_pet_name,&garden_name_16,0);
    for(int i=0;i<PET_PARTS;++i){s_pet_parts[i]=box(s_pet_root,0,0,1,1,0xA9C4A4);lv_obj_add_flag(s_pet_parts[i],LV_OBJ_FLAG_HIDDEN);}
    box(screen,20,108,200,190,0x779A73);
    for(int i=0;i<36;++i){
        int x=BOARD_X+(i%6)*TILE,y=BOARD_Y+(i/6)*TILE;
        s_tiles[i]=box(screen,x,y,28,28,0xD5DDC3);
        lv_obj_set_style_border_width(s_tiles[i],2,0);
        lv_obj_set_style_border_color(s_tiles[i],lv_color_hex(0xC2CEB3),0);
        lv_obj_add_event_cb(s_tiles[i],draw_flower,LV_EVENT_DRAW_MAIN_END,(void *)(intptr_t)i);
    }
    s_status=label(screen,12,300,"CHOOSE FLOWER",0x483D3C);
    s_battery=label(screen,199,300,"",0x483D3C);
    s_clock_view=box(screen,0,0,240,320,0xE7E0C8);
    label(s_clock_view,25,13,"SET DATE & TIME",0x483D3C);
    const char *names[5]={"YEAR","MONTH","DAY","HOUR","MINUTE"};
    for(int i=0;i<5;++i){
        int y=46+i*39;
        s_clock_rows[i]=box(s_clock_view,18,y,204,34,0xD7E3CC);
        label(s_clock_rows[i],10,8,names[i],0x483D3C);
        s_clock_values[i]=label(s_clock_rows[i],146,8,"--",0x483D3C);
    }
    label(s_clock_view,20,249,"UP/DOWN CHANGE",0x483D3C);
    label(s_clock_view,20,270,"OK NEXT  HOLD OK SAVE",0x483D3C);
    label(s_clock_view,20,291,"HOLD DOWN CANCEL",0x483D3C);
    lv_obj_add_flag(s_clock_view,LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(clock_tick,1000,NULL);
    lv_screen_load(screen);
}
static uint8_t neighbors(uint8_t index,uint8_t out[4]) {
    uint8_t count=0;
    if(index>=6)out[count++]=index-6;
    if(index%6<5)out[count++]=index+1;
    if(index<30)out[count++]=index+6;
    if(index%6>0)out[count++]=index-1;
    return count;
}
static void refresh_ui(void) {
    char line[32];uint8_t stage=garden_companion_stage(s_game.total_cleared);
    uint32_t next=garden_companion_next(s_game.total_cleared);
    uint8_t display_stage=s_preview_mode?s_preview_stage:stage;
    lv_snprintf(line,sizeof(line),s_preview_mode?"PREVIEW %u/7":"LEVEL %u/7",(unsigned)(display_stage+1));lv_label_set_text(s_level,line);
    if(s_preview_mode)lv_snprintf(line,sizeof(line),"UP/DOWN VIEW");
    else if(next)lv_snprintf(line,sizeof(line),"%lu/%lu FLOWERS",(unsigned long)s_game.total_cleared,(unsigned long)next);
    else lv_snprintf(line,sizeof(line),"%lu FLOWERS",(unsigned long)s_game.total_cleared);
    lv_label_set_text(s_target,line);
    uint32_t start=garden_companion_threshold(stage);
    uint32_t width=next?(s_game.total_cleared-start)*120/(next-start):120;
    lv_obj_set_width(s_progress,width?width:2);
    render_pet(display_stage);
    lv_label_set_text(s_pet_name,garden_companion_name(display_stage));
    lv_obj_set_x(s_pet_name,190-8*garden_companion_name_columns(display_stage));
    refresh_clock();
    uint8_t nearby[4],candidate=255;
    if(s_first!=255&&s_neighbor!=255){uint8_t count=neighbors(s_first,nearby);candidate=nearby[s_neighbor%count];}
    for(int i=0;i<36;++i){
        lv_obj_set_style_bg_color(s_tiles[i],lv_color_hex(0xD5DDC3),0);
        lv_obj_invalidate(s_tiles[i]);
        bool selected=i==s_cursor||i==s_first||i==candidate;
        uint32_t border=i==s_first?0xE89C7F:selected?0xFFF7DB:0xC2CEB3;
        lv_obj_set_style_border_color(s_tiles[i],lv_color_hex(border),0);
        lv_obj_set_style_border_width(s_tiles[i],selected?3:2,0);
    }
}
static bool restore_game(void) {
    nvs_handle_t handle;if(nvs_open("petal_garden",NVS_READONLY,&handle)!=ESP_OK)return false;
    saved_game_t saved;size_t size=sizeof(saved);
    esp_err_t err=nvs_get_blob(handle,"state",&saved,&size);nvs_close(handle);
    if(err!=ESP_OK||size!=sizeof(saved)||saved.version!=SAVE_VERSION||!saved.game.rng)return false;
    for(int i=0;i<36;++i)if(saved.game.cells[i]<1||saved.game.cells[i]>GARDEN_COLORS)return false;
    s_game=saved.game;if(garden_has_match(&s_game))return false;
    garden_rebuild_moves(&s_game);return s_game.move_count!=0;
}
static void save_game(void) {
    nvs_handle_t handle;if(nvs_open("petal_garden",NVS_READWRITE,&handle)!=ESP_OK)return;
    saved_game_t saved={.version=SAVE_VERSION,.game=s_game};
    esp_err_t err=nvs_set_blob(handle,"state",&saved,sizeof(saved));
    if(err==ESP_OK)err=nvs_commit(handle);
    if(err!=ESP_OK)ESP_LOGW(TAG,"Save failed: %s",esp_err_to_name(err));
    nvs_close(handle);
}
static void play_tone(uint16_t hz,uint16_t ms) {
    if(!s_sound_ready||!s_sound_enabled)return;
    int16_t samples[128];uint32_t total=(uint32_t)ms*16u,phase=0;
    for(uint32_t offset=0;offset<total;){
        uint32_t count=total-offset;if(count>128)count=128;
        for(uint32_t i=0;i<count;++i){
            uint32_t pos=offset+i,env=pos<80?pos:(total-pos<160?(total-pos)/2:80);
            phase=(phase+hz)%16000u;
            int32_t triangle=phase<8000u?(int32_t)phase:(int32_t)(16000u-phase);
            samples[i]=(int16_t)(((triangle-4000)*(int32_t)env)/100);
        }
        if(bsp_audio_write(samples,count*sizeof(samples[0]))!=ESP_OK){s_sound_ready=false;return;}
        offset+=count;
    }
}
static bool save_clock_edit(void) {
    if(!garden_clock_valid(&s_clock_edit))return false;
    struct tm local={.tm_year=s_clock_edit.year-1900,.tm_mon=s_clock_edit.month-1,
        .tm_mday=s_clock_edit.day,.tm_hour=s_clock_edit.hour,.tm_min=s_clock_edit.minute,.tm_isdst=-1};
    time_t seconds=mktime(&local);if(seconds==(time_t)-1)return false;
    struct timeval tv={.tv_sec=seconds,.tv_usec=0};
    if(settimeofday(&tv,NULL)!=0)return false;
    s_clock_valid=true;return true;
}
static void input_task(void *arg) {
    (void)arg;input_t input;
    for(;;){
        if(xQueueReceive(s_inputs,&input,portMAX_DELAY)!=pdTRUE)continue;
        if(s_clock_editing){
            if(input.event==BSP_BTN_LONG&&input.button==BSP_BTN_OK){
                bool saved=save_clock_edit();
                if(bsp_lvgl_lock(500)){
                    if(saved){s_clock_editing=false;lv_obj_add_flag(s_clock_view,LV_OBJ_FLAG_HIDDEN);refresh_clock();lv_label_set_text(s_status,"TIME SET");}
                    else lv_label_set_text(s_clock_values[s_clock_edit.field],"ERROR");
                    bsp_lvgl_unlock();
                }
            }else if(input.event==BSP_BTN_LONG&&input.button==BSP_BTN_DOWN){
                s_clock_editing=false;
                if(bsp_lvgl_lock(500)){lv_obj_add_flag(s_clock_view,LV_OBJ_FLAG_HIDDEN);bsp_lvgl_unlock();}
            }else if(input.event==BSP_BTN_CLICK){
                if(input.button==BSP_BTN_OK)garden_clock_next_field(&s_clock_edit);
                else garden_clock_adjust(&s_clock_edit,input.button==BSP_BTN_UP?1:-1);
                if(bsp_lvgl_lock(500)){refresh_clock_edit();bsp_lvgl_unlock();}
            }
            continue;
        }
        if(s_preview_mode){
            if(input.event==BSP_BTN_CLICK&&input.button==BSP_BTN_OK){
                s_preview_mode=false;
                if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"CHOOSE FLOWER");bsp_lvgl_unlock();}
            }else if(input.event==BSP_BTN_CLICK&&(input.button==BSP_BTN_UP||input.button==BSP_BTN_DOWN)){
                s_preview_stage=garden_preview_stage_step(s_preview_stage,input.button==BSP_BTN_UP?-1:1);
                if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"OK BACK TO GAME");bsp_lvgl_unlock();}
            }
            continue;
        }
        if(input.event==BSP_BTN_LONG&&input.button==BSP_BTN_OK){
            s_preview_stage=1;s_preview_mode=true;
            if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"OK BACK TO GAME");bsp_lvgl_unlock();}
            continue;
        }
        if(input.event==BSP_BTN_LONG&&input.button==BSP_BTN_DOWN){
            s_clock_edit=(garden_clock_edit_t){2026,1,1,12,0,0};
            if(s_clock_valid){time_t now=time(NULL);struct tm local;if(localtime_r(&now,&local)){
                s_clock_edit.year=local.tm_year+1900;s_clock_edit.month=local.tm_mon+1;
                s_clock_edit.day=local.tm_mday;s_clock_edit.hour=local.tm_hour;s_clock_edit.minute=local.tm_min;
            }}
            s_clock_editing=true;
            if(bsp_lvgl_lock(500)){refresh_clock_edit();lv_obj_remove_flag(s_clock_view,LV_OBJ_FLAG_HIDDEN);bsp_lvgl_unlock();}
            continue;
        }
        if(input.event==BSP_BTN_LONG&&input.button==BSP_BTN_UP){
            s_sound_enabled=!s_sound_enabled;
            if(bsp_lvgl_lock(500)){lv_label_set_text(s_status,s_sound_enabled?"SOUND ON":"SOUND OFF");bsp_lvgl_unlock();}
            continue;
        }
        if(input.event!=BSP_BTN_CLICK)continue;
        if(input.button==BSP_BTN_UP||input.button==BSP_BTN_DOWN){
            int direction=input.button==BSP_BTN_UP?-1:1;
            if(s_first==255)s_cursor=(uint8_t)((s_cursor+36+direction)%36);
            else{uint8_t nearby[4],count=neighbors(s_first,nearby);
                s_neighbor=s_neighbor==255?(direction<0?count-1:0):(uint8_t)((s_neighbor+count+direction)%count);}
            if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,s_first==255?"CHOOSE FLOWER":"PICK NEIGHBOR");bsp_lvgl_unlock();}
            continue;
        }
        if(input.button!=BSP_BTN_OK)continue;
        if(s_first==255){s_first=s_cursor;s_neighbor=255;
            if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"PICK NEIGHBOR");bsp_lvgl_unlock();}
            continue;
        }
        if(s_neighbor==255){if(bsp_lvgl_lock(500)){lv_label_set_text(s_status,"PICK NEIGHBOR");bsp_lvgl_unlock();}continue;}
        uint8_t nearby[4],count=neighbors(s_first,nearby),a=s_first,b=nearby[s_neighbor%count];
        bool marks[36];uint8_t matched=garden_preview_swap(&s_game,a,b,marks);
        uint8_t original[36];memcpy(original,s_game.cells,sizeof(original));
        s_first=255;s_neighbor=255;s_cursor=b;
        s_game.cells[a]=original[b];s_game.cells[b]=original[a];
        if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"CHECK 3 SAME");bsp_lvgl_unlock();}
        vTaskDelay(pdMS_TO_TICKS(220));
        if(!matched){
            play_tone(330,65);memcpy(s_game.cells,original,sizeof(original));s_game.last_cleared=0;
            if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"NO MATCH");bsp_lvgl_unlock();}
            continue;
        }
        if(bsp_lvgl_lock(500)){for(int i=0;i<36;++i)if(marks[i])lv_obj_set_style_bg_color(s_tiles[i],lv_color_hex(0xFFF0C8),0);
            lv_label_set_text(s_status,"3+ FLOWERS MATCH");bsp_lvgl_unlock();}
        play_tone(659,80);vTaskDelay(pdMS_TO_TICKS(450));
        for(int i=0;i<36;++i)if(marks[i])s_game.cells[i]=0;
        if(bsp_lvgl_lock(500)){refresh_ui();bsp_lvgl_unlock();}
        vTaskDelay(pdMS_TO_TICKS(170));memcpy(s_game.cells,original,sizeof(original));
        uint8_t old_stage=garden_companion_stage(s_game.total_cleared);
        bool played=garden_try_swap(&s_game,a,b);
        if(played){save_game();uint8_t new_stage=garden_companion_stage(s_game.total_cleared);
            ESP_LOGI(TAG,"move cleared=%lu total=%lu stage=%u",(unsigned long)s_game.last_cleared,(unsigned long)s_game.total_cleared,(unsigned)new_stage+1);
            if(new_stage>old_stage)play_tone(784,100);
            if(bsp_lvgl_lock(500)){
                refresh_ui();
                char result[32];
                if(new_stage>old_stage)lv_snprintf(result,sizeof(result),"LEVEL %u CLEAR!",(unsigned)old_stage+1);
                else lv_snprintf(result,sizeof(result),"+%lu FLOWERS",(unsigned long)s_game.last_cleared);
                lv_label_set_text(s_status,result);
                bsp_lvgl_unlock();
            }
        }else{
            ESP_LOGE(TAG,"preview and model disagreed for swap %u,%u",(unsigned)a,(unsigned)b);
            if(bsp_lvgl_lock(500)){refresh_ui();lv_label_set_text(s_status,"TRY AGAIN");bsp_lvgl_unlock();}
        }
    }
}
static void on_button(bsp_btn_t button,bsp_btn_ev_t event,void *user) {
    (void)user;if(!s_ready||!s_inputs||(event!=BSP_BTN_CLICK&&event!=BSP_BTN_LONG))return;
    input_t input={button,event};(void)xQueueSend(s_inputs,&input,0);
}
void app_main(void) {
    esp_err_t err=nvs_flash_init();if(err!=ESP_OK)ESP_LOGW(TAG,"Save storage unavailable: %s",esp_err_to_name(err));
    if(!restore_game())garden_init(&s_game,esp_random());
    setenv("TZ","CST-8",1);tzset();
    bsp_i2c_init();
    if(bsp_audio_init()==ESP_OK&&bsp_audio_set_format(16000,16,1)==ESP_OK){bsp_audio_set_volume(22);s_sound_ready=true;}
    if(bsp_battery_init()!=ESP_OK)ESP_LOGW(TAG,"Battery level unavailable");
    if(bsp_display_init()!=ESP_OK||!bsp_lvgl_init()){ESP_LOGE(TAG,"Display unavailable");return;}
    bsp_display_backlight(80);
    int battery=bsp_battery_soc();
    if(!bsp_lvgl_lock(1000))return;
    lv_font_glyph_dsc_t glyph;
    const char *const names="象芽小耳朵稳站卷鼻子一花冠七彩";
    for(const unsigned char *p=(const unsigned char *)names;*p;p+=3){
        uint32_t letter=((uint32_t)(p[0]&0x0f)<<12)|((uint32_t)(p[1]&0x3f)<<6)|(p[2]&0x3f);
        if(!lv_font_get_glyph_dsc(&garden_name_16,&glyph,letter,0)||glyph.is_placeholder)
            ESP_LOGE(TAG,"Companion name font lacks U+%04lx",(unsigned long)letter);
    }
    build_ui();refresh_ui();
    if(battery>=0){char line[12];lv_snprintf(line,sizeof(line),"%d%%",battery);lv_label_set_text(s_battery,line);}
    bsp_lvgl_unlock();
    s_inputs=xQueueCreate(INPUT_QUEUE_DEPTH,sizeof(input_t));
    if(!s_inputs||xTaskCreate(input_task,"garden_input",4096,NULL,5,NULL)!=pdPASS||bsp_button_init(on_button,NULL)!=ESP_OK){ESP_LOGE(TAG,"Buttons unavailable");return;}
    s_ready=true;ESP_LOGI(TAG,"Offline petal garden with local clock ready");
}
