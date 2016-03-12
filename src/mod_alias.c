#include "module.h"
#include "stb_sb.h"
#include <string.h>
#include <ctype.h>
#include "utils.h"

static void alias_msg  (const char*, const char*, const char*);
static void alias_cmd  (const char*, const char*, const char*, int);
static bool alias_save (FILE*);
static bool alias_init (const IRCCoreCtx*);

enum { ALIAS_ADD, ALIAS_DEL, ALIAS_LIST, ALIAS_SET_PERM };

const IRCModuleCtx irc_mod_ctx = {
	.name     = "alias",
	.desc     = "Allows defining simple responses to !commands",
	.flags    = IRC_MOD_DEFAULT,
	.on_save  = &alias_save,
	.on_msg   = &alias_msg,
	.on_cmd   = &alias_cmd,
	.on_init  = &alias_init,
	.commands = DEFINE_CMDS (
		[ALIAS_ADD]      = CONTROL_CHAR"alias ",
		[ALIAS_DEL]      = CONTROL_CHAR"unalias "    CONTROL_CHAR"delalias "   CONTROL_CHAR"rmalias ",
		[ALIAS_LIST]     = CONTROL_CHAR"lsalias "    CONTROL_CHAR"lsa "        CONTROL_CHAR"listalias "    CONTROL_CHAR"listaliases ",
		[ALIAS_SET_PERM] = CONTROL_CHAR"chaliasmod " CONTROL_CHAR"chamod "       CONTROL_CHAR"aliasaccess "  CONTROL_CHAR"setaliasaccess "
	)
};

static const IRCCoreCtx* ctx;

enum {
    AP_NORMAL = 0,
    AP_WHITELISTED,
    AP_ADMINONLY,
       
    AP_COUNT,
};

char* alias_permission_strs[] = {
    "NORMAL",
    "WLIST",
    "ADMIN",
};

typedef struct {
    int permission;
    char* msg;
} Alias;

//TODO: aliases should be per-channel
static char** alias_keys;
static Alias* alias_vals;

static void alias_add(char* key, char* msg, int perm) {
    fprintf(stderr, "Got alias: [%s] = [%s] (Access level %s)\n", key, msg, alias_permission_strs[perm]);
    //TODO(chronister): Allocate this dynamically, so that alias_vals can be an array of pointers
    // and we end up with multiple keys going to actually the same values
    Alias value = {
        .permission = perm,
        .msg        = msg,
    };
    sb_push(alias_keys, key);
    sb_push(alias_vals, value);
}

static bool alias_init(const IRCCoreCtx* _ctx){
	ctx = _ctx;
	FILE* f = fopen(ctx->get_datafile(), "rb");

	char *key, *msg, *permstr;
    int perm = AP_NORMAL;

    bool got_alias = false;
	do {
        size_t fptr = ftell(f);
        // Alias format: [PERM] KEY VALUE
        // Bot saves PERM automatically now so it should only find two-argument aliases
        // in files saved before this module added permissions
        got_alias = fscanf(f, "%ms %ms %m[^\n]", &permstr, &key, &msg) == 3;
        if (got_alias) {
            if      (strcmp(permstr, alias_permission_strs[AP_NORMAL]) == 0)        perm = AP_NORMAL;
            else if (strcmp(permstr, alias_permission_strs[AP_WHITELISTED]) == 0)   perm = AP_WHITELISTED;
            else if (strcmp(permstr, alias_permission_strs[AP_ADMINONLY]) == 0)     perm = AP_ADMINONLY;
            // Unknown prefix, assume "prefix" is actually the key of an old-style alias declaration
            else got_alias = false;
        }
        if (!got_alias) {
            // Try again looking for an old-style declaration this time
            fseek(f, fptr, SEEK_SET);
            got_alias = fscanf(f, "%ms %m[^\n]", &key, &msg) == 2;
            perm = AP_NORMAL;
        }

        if (got_alias) {
            alias_add(key, msg, perm);
        }
	} while(got_alias);
		
	return true;
}

static void whitelist_cb(intptr_t result, intptr_t arg){
	if(result) *(bool*)arg = true;
}

static void alias_cmd(const char* chan, const char* name, const char* arg, int cmd){

	bool has_cmd_perms = strcasecmp(chan+1, name) == 0;
	if(!has_cmd_perms){
		MOD_MSG(ctx, "check_whitelist", name, &whitelist_cb, &has_cmd_perms);
	}
	if(!has_cmd_perms) return;

	switch(cmd){
		case ALIAS_ADD: {
			if(!*arg++ || !isalnum(*arg)) goto usage_add;

			const char* space = strchr(arg, ' ');
			if(!space) goto usage_add;

			char* key = strndupa(arg, space - arg);
			for(char* k = key; *k; ++k) *k = tolower(*k);

			bool found = false;
			for(int i = 0; i < sb_count(alias_keys); ++i){
				if(strcmp(key, alias_keys[i]) == 0){
					free(alias_vals[i].msg);
					alias_vals[i].msg = strdup(space+1);
					found = true;
				}
			}

			if(!found){
                alias_add(strdup(key), strdup(space+1), AP_NORMAL);
			}

			ctx->send_msg(chan, "%s: Alias %s set.", name, key);
		} break;

		case ALIAS_DEL: {
			if(!*arg++ || !isalnum(*arg)) goto usage_del;

			bool found = false;
			for(int i = 0; i < sb_count(alias_keys); ++i){
				if(strcasecmp(arg, alias_keys[i]) == 0){
					found = true;

					free(alias_keys[i]);
					sb_erase(alias_keys, i);
					
					free(alias_vals[i].msg);
					sb_erase(alias_vals, i);

					ctx->send_msg(chan, "%s: Removed alias %s.\n", name, arg);
					break;
				}
			}

			if(!found){
				ctx->send_msg(chan, "%s: That alias doesn't exist.", name);
			}
		} break;

		case ALIAS_LIST: {
			char alias_buf[512];
			char* ptr = alias_buf;
			size_t sz = sizeof(alias_buf);

			const size_t total = sb_count(alias_keys);

			if(total == 0){
				inso_strcat(alias_buf, sizeof(alias_buf), "<none>.");
			} else {
				for(int i = 0; i < total; ++i){
					snprintf_chain(&ptr, &sz, "!%s%s", alias_keys[i], i == (total-1) ? "." : ", ");
				}
			}

			ctx->send_msg(chan, "%s: Current aliases: %s", name, alias_buf);
		} break;

        case ALIAS_SET_PERM: {
			if(!*arg++ || !isalnum(*arg)) goto usage_setperm;
            
			const char* space = strchr(arg, ' ');
			if(!space) goto usage_setperm;

			char* key = strndupa(arg, space - arg);
			for(char* k = key; *k; ++k) *k = tolower(*k);

            Alias* alias;
			bool found = false;
			for(int i = 0; i < sb_count(alias_keys); ++i){
				if(strcmp(key, alias_keys[i]) == 0){
					found = true;
                    alias = alias_vals + i;
				}
			}

			if(!found){
                ctx->send_msg(chan, "%s: No alias called '%s'.", name, key);
                return;
			}

            int perm = -1;
            const char* permstr = space+1;             
            if (strcasecmp(permstr, alias_permission_strs[AP_NORMAL]) == 0)
                perm = AP_NORMAL;
            else if (strcasecmp(permstr, alias_permission_strs[AP_WHITELISTED]) == 0)
                perm = AP_WHITELISTED;
            else if (strcasecmp(permstr, alias_permission_strs[AP_ADMINONLY]) == 0)
                perm = AP_ADMINONLY;
            
            if (perm == -1) {
                ctx->send_msg(chan, "%s: Not sure what permission level '%s' is.", name, permstr);
                return;
            }
                
            alias->permission = perm;
			ctx->send_msg(chan, "%s: Set permissions on %s to %s.", name, key, permstr);
        } break;
	}

	ctx->save_me();

	return;

usage_add:
	ctx->send_msg(chan, "%s: Usage: "CONTROL_CHAR"alias <key> <text>", name); return;
usage_del:
	ctx->send_msg(chan, "%s: Usage: "CONTROL_CHAR"unalias <key>", name); return;
usage_setperm:
	ctx->send_msg(chan, "%s: Usage: "CONTROL_CHAR"chaliasmod <key> [NORMAL|WLIST|ADMIN]", name); return;
}

static void alias_msg(const char* chan, const char* name, const char* msg){

	if(*msg != '!') return;

	int index = -1;
	const char* arg = NULL;
	size_t arg_len = 0;

	for(int i = 0; i < sb_count(alias_keys); ++i){
		size_t alias_len = strlen(alias_keys[i]);

		if(strncasecmp(msg + 1, alias_keys[i], alias_len) == 0){
			index = i;
			arg = msg + alias_len + 1;

			while(*arg == ' '){
				++arg;
			}

			if(*arg){
				arg_len = strlen(arg);
			}

			break;
		}
	}
	if(index < 0) return;

	size_t name_len = strlen(name);
	char* msg_buf = NULL;

    Alias value = alias_vals[index];
	bool has_cmd_perms = (value.permission == AP_NORMAL) || strcasecmp(chan+1, name) == 0;
	if(!has_cmd_perms){
        if (value.permission == AP_WHITELISTED)
            MOD_MSG(ctx, "check_whitelist", name, &whitelist_cb, &has_cmd_perms);
        else if (value.permission == AP_ADMINONLY)
            MOD_MSG(ctx, "check_admin", name, &whitelist_cb, &has_cmd_perms);
        else {
            // Some kind of weird unknown permission type. Assume normal access.
            has_cmd_perms = true;
        }
	}
	if(!has_cmd_perms) return;

	for(const char* str = value.msg; *str; ++str){
		if(*str == '%' && *(str + 1) == 't'){
			memcpy(sb_add(msg_buf, name_len), name, name_len);
			++str;
		} else if(*str == '%' && *(str + 1) == 'a'){
			if(arg && *arg && arg_len){
				memcpy(sb_add(msg_buf, arg_len), arg, arg_len);
			}
			++str;
		} else if(*str == '%' && *(str + 1) == 'n'){
			if(arg && *arg && arg_len){
				memcpy(sb_add(msg_buf, arg_len), arg, arg_len);
			} else {
				memcpy(sb_add(msg_buf, name_len), name, name_len);
			}
			++str;
		} else {
			sb_push(msg_buf, *str);
		}
	}

	sb_push(msg_buf, 0);
	ctx->send_msg(chan, "%s", msg_buf);

	sb_free(msg_buf);
}

static bool alias_save(FILE* file){
	for(int i = 0; i < sb_count(alias_keys); ++i){
        bool perm_valid = (alias_vals[i].permission >= 0 && alias_vals[i].permission <= AP_COUNT);
        if (perm_valid) {
            fprintf(file, "%s\t", alias_permission_strs[alias_vals[i].permission]);
        }
        fprintf(file, "%s\t%s\n", alias_keys[i], alias_vals[i].msg);
	}
	return true;
}

