#include "module.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum { EIGHTBALL, BEEP, ROLLDICE, RANDOM, TROLL,  };

static void bonus_cmd(const char* chan, const char* name, const char* msg, int cmd);
static bool bonus_init(const IRCCoreCtx* _ctx);

const IRCModuleCtx irc_mod_ctx = {
    .name       = "bonus",
    .desc       = "Just-for-fun features",
    .flags      = IRC_MOD_DEFAULT,
    .on_cmd     = &bonus_cmd,
	.on_init    = &bonus_init,
    .commands   = DEFINE_CMDS (
        [EIGHTBALL] = CONTROL_CHAR"8 "CONTROL_CHAR"8ball "CONTROL_CHAR"fortune",
        [BEEP] = CONTROL_CHAR"beep "CONTROL_CHAR"boop",
        [ROLLDICE] = CONTROL_CHAR"roll "CONTROL_CHAR"rolldice "CONTROL_CHAR"r",
        [RANDOM] = CONTROL_CHAR"random",
        [TROLL] = CONTROL_CHAR"troll "CONTROL_CHAR"flame "CONTROL_CHAR"holywar"
    )
};

static const IRCCoreCtx* ctx;

#define countof(x) (sizeof(x) / sizeof((x)[0]))
#define rand_choice(arr) ((arr)[rand() % countof(arr)])

static const char* eightball_phrases[] = {
    "It is certain",
    "It is decidedly so",
    "Without a doubt",
    "Yes, definitely",
    "You may rely on it",
    "As I see it, yes",
    "Most likely",
    "Outlook good",
    "Yes",
    "Signs point to yes",
    "Reply hazy try again",
    "Ask again later",
    "Better not tell you now",
    "Cannot predict now",
    "Concentrate and ask again",
    "Don't count on it",
    "My reply is no",
    "My sources say no",
    "Outlook not so good",
    "Very doubtful"
};

static const char* beep_phrases[] = {
    "Don't speak of my mother that way!",
    "That command was deprecated as of version 1.6.7, please use 1.3.1 for a more updated API",
    "o/",
    "Pushing random buttons isn't meaningful communication, you know!",
    "What goes around, comes around",
    "Do it again. I dare you.",
    "What good is an IRC bot without easter egg commands?",
    "You win! Play again?",
    "Beeeeeeeeeep",
    "Who do you think you are!?",
    "Fatal Error: Sarcastic response not found",
    "What if you're the bot and I'm the human?",
    "kknewkles--",
    "The things I have to put up with around here..."
};

enum parse_dice {
    DICE_UNKNOWN,
    DICE_NUMDICE,
    DICE_NUMFACES,
    DICE_NUMDROPS
};

static void wlist_check_cb(intptr_t result, intptr_t arg){
	if(result) *(bool*)arg = true;
}

static void bonus_cmd(const char* chan, const char* name, const char* msg, int cmd)
{
    bool can_use_bonus = false;
    MOD_MSG(ctx, "check_whitelist", name, &wlist_check_cb, &can_use_bonus);
    if (!can_use_bonus) return;

    switch(cmd) 
    {
        case EIGHTBALL:
        {
            ctx->send_msg(chan, "%s: %s", name, rand_choice(eightball_phrases));
        } break;

        case BEEP:
        {
            ctx->send_msg(chan, "%s", rand_choice(beep_phrases));
        } break;

        case RANDOM:
        {
            int rand_num = ((rand() % 16) * 2+7) / 13/3 + 4;
            ctx->send_msg(chan, "%s: Your random number is %d", name, rand_num);
        } break;

        case ROLLDICE:
        {
            // !roll [# of dice, int]d(# of faces per dice)[d(# of lowest dice to drop)]
            enum parse_dice parse_state = DICE_NUMDICE;
            char* num_start = strdupa(msg);
            struct {
                int dice;
                int faces;
                int drops;
            } dice_params;
            dice_params.dice = -1;
            dice_params.faces = -1;
            dice_params.drops = -1;
            for (char* cur = num_start; ; ++cur) {
                if (parse_state == DICE_NUMDICE) {
                    if (isspace(*cur)) { continue; }
                }

                if (*cur == 'd') {
                    switch(parse_state) {
                        case DICE_NUMDICE: {
                            *cur = '\0';
                            int scanned = sscanf(num_start, "%u", &dice_params.dice);
                            if (scanned != 1) {
                                ctx->send_msg(chan, "I don't have %s dice just lying around!", num_start);
                                return;
                            }
                            num_start = cur + 1;
                            parse_state = DICE_NUMFACES; 
                            continue;
                       } break;
                        case DICE_NUMFACES: {
                            *cur = '\0';
                            int scanned = sscanf(num_start, "%u", &dice_params.faces);
                            if (scanned != 1) {
                                ctx->send_msg(chan, "I don't have any dice with %s faces.", num_start);
                                return;
                            }
                            num_start = cur + 1;
                            parse_state = DICE_NUMDROPS; 
                            continue;
                        } break;
                        default:
                            break;
                    }
                }

                if (*cur == '\0' || isspace(*cur)) {
                    if (parse_state == DICE_NUMDROPS) {
                        *cur = '\0';
                        int scanned = sscanf(num_start, "%u", &dice_params.drops);
                        if (scanned != 1) {
                            ctx->send_msg(chan, "I can't drop %s dice", num_start);
                            return;
                        }
                    }
                    else if (parse_state == DICE_NUMFACES) {
                        *cur = '\0';
                        int scanned = sscanf(num_start, "%u", &dice_params.faces);
                        if (scanned != 1) {
                            ctx->send_msg(chan, "I don't have any dice with %s faces.", num_start);
                            return;
                        }
                    }
                    parse_state = DICE_UNKNOWN;
                }

                if (parse_state == DICE_UNKNOWN) {
                    break;
                }
            }

            if (dice_params.faces < 0) {
                ctx->send_msg(chan, "I don't have any dice with %d faces...", dice_params.dice);
                return;
            }
            if (dice_params.dice < 0) {
                dice_params.dice = 1;
            }
            if (dice_params.drops < 0) {
                dice_params.drops = 0;
            }

            if (dice_params.dice > 20) {
                char* things = "dice";
                if (dice_params.faces == 2) things = "coins";
                if (dice_params.faces == 3) things = "thick cylinders";
                if (dice_params.faces == 1) {
                    if (rand() % 2 == 1) things = "mobius strips";
                    else things = "spheres";
                }
                ctx->send_msg(chan, "Do you think I have %d %s just lying around!?", dice_params.dice, things);
                return;
            }
            if (dice_params.faces > 100) {
                ctx->send_msg(chan, "I rolled the sphere, and it rolled off the table!");
                return;
            }
            if (dice_params.drops > dice_params.dice) {
                ctx->send_msg(chan, "Whoops, I dropped all the dice!");
                return;
            }

            int dice_rolls[dice_params.dice];
            // Indices into dice_rolls
            int dice_rolls_sorted[dice_params.dice];

            for (int dice_index = 0; dice_index < dice_params.dice; ++dice_index) {
                int dice_val = rand() % dice_params.faces + 1;
                dice_rolls[dice_index] = dice_val;

                for (int roll_index = 0; roll_index <= dice_index; ++roll_index) {
                    if (roll_index == dice_index) {
                        dice_rolls_sorted[roll_index] = dice_index;
                        break;
                    }
                    if (dice_rolls[dice_rolls_sorted[roll_index]] > dice_val) {
                        memmove(dice_rolls_sorted + roll_index + 1, 
                               dice_rolls_sorted + roll_index, 
                               sizeof(int) * (dice_index - roll_index));
                        dice_rolls_sorted[roll_index] = dice_index;
                        break;
                    }
                }
            }

            // dice*3: max possible face string size (3 == decimal digits in 100)
            // 3*dice: account for '[', ']', and ' '
            // 20*3: max possible dice_total string size
            int max_string_size = dice_params.dice * 3 + 3*dice_params.dice + 20 + 20*3;
            char outstr[max_string_size];
            int total = 0;
            int printed = 0;
            for (int roll_index = 0; roll_index < dice_params.dice; ++roll_index) {
                bool dropped = false;
                for (int sorted_index = 0; sorted_index < dice_params.drops; ++sorted_index) {
                    if (roll_index == dice_rolls_sorted[sorted_index]) {
                        dropped = true;
                        break;
                    }
                }
                if (dropped) continue;

                int justprinted = 
                    snprintf(outstr+printed, max_string_size-printed, "[%d] ", dice_rolls[roll_index]);
                if (justprinted > 0)
                    printed += justprinted;
                total += dice_rolls[roll_index];
            }

            outstr[printed-1] = ',';
            snprintf(outstr+printed, max_string_size-printed, " for a total of [%d]", total);
            ctx->send_msg(chan, "%s: %s", name, outstr);
        } break;

        default:
        {
            ctx->send_msg(chan, "Not implemented yet, try again later!");
        } break;
    }
}

static bool bonus_init(const IRCCoreCtx* _ctx){
	ctx = _ctx;
	return true;
}
