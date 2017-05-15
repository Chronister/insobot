#include "module.h"
#include "config.h"
#include <regex.h>
#include <curl/curl.h>
#include "inso_utils.h"
#include "stb_sb.h"

enum { GOOGLE, DDG, MSDN, };

static void search_cmd(const char* chan, const char* name, const char* msg, int cmd);
static bool search_init(const IRCCoreCtx* _ctx);

const IRCModuleCtx irc_mod_ctx = {
    .name       = "search",
    .desc       = "Quick web search commands",
    .flags      = IRC_MOD_DEFAULT,
    .on_cmd     = &search_cmd,
    .on_init    = &search_init,
    .commands   = DEFINE_CMDS (
        [GOOGLE] = CONTROL_CHAR"g "CONTROL_CHAR"google",
        [DDG]    = CONTROL_CHAR"ddg "CONTROL_CHAR"duckduckgo",
        [MSDN]   = CONTROL_CHAR"msdn"
    )
};

static const IRCCoreCtx* ctx;

static regex_t ddg_result_regex;

static bool search_init(const IRCCoreCtx* _ctx){
    ctx = _ctx;

    bool ret = true;
    int reg_result = regcomp(&ddg_result_regex, "<a class=\"result__snippet\"\\s+href=\"([^\"]*)\"", REG_EXTENDED | REG_ICASE);
    if (reg_result != 0) {
        ret = false;
        char errbuf[256];
        regerror(reg_result, &ddg_result_regex, errbuf, 256);
        fprintf(stderr, "Couldn't compile search regex: %s\n", errbuf);
    }
    else {
        fprintf(stderr, "Search regex compiled succesfully\n");
    }

    return ret;
}


static void wlist_check_cb(intptr_t result, intptr_t arg){
    if(result) *(bool*)arg = true;
}

static void search_cmd(const char* chan, const char* name, const char* msg, int cmd)
{
    bool can_use_bonus = false;
    MOD_MSG(ctx, "check_whitelist", name, &wlist_check_cb, &can_use_bonus);
    if (!can_use_bonus) return;

    char* ddg_url;
    char* encoded;
    CURL* tempcurl = curl_easy_init();
    int urlres = asprintf(&ddg_url, "https://duckduckgo.com/html/?q=%s%s&kl=wt-wt&kz=-1&kaf=1&kd=-1&k1=-1&t=hmd_bot", 
                         encoded = curl_easy_escape(tempcurl, msg, 0),
                         (cmd == MSDN) ? "%20site:msdn.microsoft.com" : "");
    curl_free(encoded);
    curl_easy_cleanup(tempcurl);
    if (urlres == -1) { return; }

    char* data = NULL;
    CURL* curl = inso_curl_init(ddg_url, &data);
    int curl_ret = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 303) {
        char* redir = NULL;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redir);

        ctx->send_msg(chan, "%s: %s", name, redir);
        return;
    }

    curl_easy_cleanup(curl);

    sb_push(data, 0);

    if(curl_ret != 0){
        ctx->send_msg(chan, "%s: Couldn't connect to the search engine!", name);
        fprintf(stderr, "Error getting search results: %s\n", curl_easy_strerror(curl_ret));
        free(ddg_url);
        sb_free(data);
        return;
    }

    regmatch_t urlmatch[2];

    if(regexec(&ddg_result_regex, data, 2, urlmatch, 0) == 0){
        if (urlmatch[1].rm_so != -1) {
            char* result = strndupa(data + urlmatch[1].rm_so, 
                                    urlmatch[1].rm_eo - urlmatch[1].rm_so);
            ctx->send_msg(chan, "%s: %s", name, result);
        }
        else {
            ctx->send_msg(chan, 
                          "%s: I wasn't sure how to find a result, blame ChronalDragon", name);
        }
    } else {
            ctx->send_msg(chan, 
                          "%s: I wasn't sure how to find a result, blame ChronalDragon", name);
    }
    
    free(ddg_url);
    sb_free(data);
}
