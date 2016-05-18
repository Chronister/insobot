#include "module.h"
#include "config.h"
#include "stb_sb.h"
#include <string.h>
#include <ctype.h>

enum { YTMND, YTWND, YTPND, YATMND, YATWND, YATPND, WTMND, WTWND, WTPND };

static void ytmnd_cmd(const char* chan, const char* name, const char* msg, int cmd);
static bool ytmnd_init(const IRCCoreCtx* _ctx);

const IRCModuleCtx irc_mod_ctx = {
    .name       = "ytmnd",
    .desc       = "Tracking who is the man now, dog?",
    .flags      = IRC_MOD_DEFAULT,
    .on_cmd     = &ytmnd_cmd,
	.on_init    = &ytmnd_init,
    .commands   = DEFINE_CMDS (
        [YTMND] = CONTROL_CHAR"ytmnd",
        [YTWND] = CONTROL_CHAR"ytwnd",
        [YTPND] = CONTROL_CHAR"ytpnd",
        [YATMND] = CONTROL_CHAR"yatmnd",
        [YATWND] = CONTROL_CHAR"yatwnd",
        [YATPND] = CONTROL_CHAR"yatpnd",
        [WTMND] = CONTROL_CHAR"wtmnd",
        [WTWND] = CONTROL_CHAR"wtwnd",
        [WTPND] = CONTROL_CHAR"wtpnd"
    )
};

static const IRCCoreCtx* ctx;

#define DOG_M 0x1
#define DOG_F 0x2
#define DOG_N 0x3

struct PersonNowDog {
    char* name;
    int gender;
};

static struct PersonNowDog* people_now_dogs = NULL;

static void add_person_now_dog(char* name, int gender, bool clear)
{
    if (clear) {
        for (int person_index = 0; person_index < sb_count(people_now_dogs); ++person_index) {
            free(people_now_dogs[person_index].name);
        }
        sb_free(people_now_dogs);
        people_now_dogs = NULL;
    }

    struct PersonNowDog dog = {
        .name = name,
        .gender = gender,
    };
    sb_push(people_now_dogs, dog);
}

static void print_who_is_the_person_now_dog(const char* chan) 
{
    char* people_str = NULL;
    int gender = 0;
    if (sb_count(people_now_dogs) > 0) {
        for (int person_index = 0; 
            person_index < sb_count(people_now_dogs); 
            ++person_index) {
            struct PersonNowDog person = people_now_dogs[person_index];
            gender |= person.gender;
            strcpy(sb_add(people_str, strlen(person.name)+1), person.name);
            sb_pop(people_str);
            strcpy(sb_add(people_str, 3), ", ");
            sb_pop(people_str);
        }
        people_str[sb_count(people_str) - 2] = ':';
        sb_push(people_str, '\0');

        char* who_str;
        char* dog_str;
        if (sb_count(people_now_dogs) <= 1) {
            if (gender == DOG_M) who_str = "man";
            else if (gender == DOG_F) who_str = "woman";   
            else who_str = "person";
            dog_str = "dog";
        }
        else {
            if (gender == DOG_M) who_str = "men";
            else if (gender == DOG_F) who_str = "women";   
            else who_str = "people";
            dog_str = "dogs";
        }

        ctx->send_msg(chan, "%sYou're the %s now, %s!", people_str, who_str, dog_str);
    } else {
        ctx->send_msg(chan, "No one is the man now, dog :(");
    }
}

static void ytmnd_cmd(const char* chan, const char* name, const char* msg, int cmd)
{
    int gender = 0;
    bool clear = false;
    switch(cmd) 
    {
        case WTMND:
        case WTWND:
        case WTPND:
        {
            print_who_is_the_person_now_dog(chan);
            return;
        } break;

        case YTMND:
        {
            gender = DOG_M;
            clear = true;
        } break;

        case YTWND:
        {
            gender = DOG_F;
            clear = true;
        } break;

        case YTPND:
        {
            gender = DOG_N;
            clear = true;
        } break;

        case YATMND:
        {
            gender = DOG_M;
            clear = false;
        } break;

        case YATWND:
        {
            gender = DOG_F;
            clear = false;
        } break;

        case YATPND:
        {
            gender = DOG_N;
            clear = false;
        } break;

        default:
        {
            return;
        } break;
    }
    
    bool dogging = true;
    const char* Start = msg;
    do {
        while (*Start && isspace(*Start)) ++Start;
        const char* End = Start;
        while (*End && !isspace(*End)) ++End;

        if (End != Start) {
            add_person_now_dog(strndup(Start, Start - End), gender, clear);
            clear = false;
        } else {
            dogging = false;
        }
        Start = End;
    } while(dogging);

    print_who_is_the_person_now_dog(chan);
}

static bool ytmnd_init(const IRCCoreCtx* _ctx){
	ctx = _ctx;
	return true;
}
